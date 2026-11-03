
#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "configuration.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QThread>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

//*****************************************************************************************************

// Test instrumentation for the headless bench (#105).  CM_TEST_FAULTS, when set, makes numbered HTTP requests of this run
// answer as if Google had sent them, without contacting Google.  "w" numbers the writes (uploads, creates, changes and
// deletes) and "r" the reads, each from 1 in the order sent.  Format:
//   "w3=403:rateLimitExceeded;w5-6=429:rateLimitExceeded;r2=400:badRequest;w9=closed"
// A status of "closed" is a connection closed by Google.  It is unset in normal use.
struct TestFault { char kind ; int first, last, status ; QString reason ; } ;
static int writerequestcount = 0 ;
static int readrequestcount = 0 ;

static const QList<TestFault> &testFaultPlan()
{
    static QList<TestFault> plan ;
    static bool parsed = false ;
    if (!parsed) {
        parsed = true ;
        for (const QString &item : qEnvironmentVariable("CM_TEST_FAULTS").split(';', Qt::SkipEmptyParts)) {
            QStringList parts = item.split('=') ;
            if (parts.size()!=2 || parts.at(0).size()<2) continue ;
            TestFault f ;
            f.kind = parts.at(0).at(0).toLatin1() ;
            QStringList range = parts.at(0).mid(1).split('-') ;
            f.first = range.at(0).toInt() ;
            f.last = range.size()>1 ? range.at(1).toInt() : f.first ;
            if (parts.at(1)=="closed") {
                f.status = -1 ;
                f.reason = "closed" ;
            } else {
                QStringList answer = parts.at(1).split(':') ;
                if (answer.size()!=2) continue ;
                f.status = answer.at(0).toInt() ;
                f.reason = answer.at(1) ;
            }
            plan.append(f) ;
        }
    }
    return plan ;
}

// The fault for the numbered request of this kind ('w' or 'r'), or NULL
static const TestFault *testFaultFor(char kind, int request)
{
    for (const TestFault &f : testFaultPlan()) {
        if (f.kind==kind && request>=f.first && request<=f.last) return &f ;
    }
    return NULL ;
}

// The body of the reply: the injected error (as Google writes it) when a fault is given, otherwise what was received
static QByteArray replyBody(QNetworkReply *reply, const TestFault *fault)
{
    if (!fault) return reply->readAll() ;
    QJsonObject item ;
    item["reason"] = fault->reason ;
    item["message"] = QString("Injected by the bench: %1").arg(fault->reason) ;
    QJsonArray errors ;
    errors.append(item) ;
    QJsonObject inner ;
    inner["code"] = fault->status ;
    inner["message"] = item["message"] ;
    inner["errors"] = errors ;
    QJsonObject root ;
    root["error"] = inner ;
    return QJsonDocument(root).toJson() ;
}


// Google's reason for a failed request, and its message, from the error body it sends (issue #94)
static void googleReason(const QByteArray &body, QString &reason, QString &message)
{
    reason.clear() ;
    message.clear() ;
    QJsonObject error = QJsonDocument::fromJson(body).object().value("error").toObject() ;
    QJsonArray errors = error.value("errors").toArray() ;
    if (!errors.isEmpty()) {
        reason = errors.at(0).toObject().value("reason").toString() ;
        message = errors.at(0).toObject().value("message").toString() ;
    }
    if (message.isEmpty()) message = error.value("message").toString() ;
}

// Only these are retried: Google says the limit will clear (issue #94).  Authentication, permission, not-found and mismatch
// errors are not.
static bool isRateLimit(int status, const QString &reason)
{
    return status==429 || (status==403 && (reason=="rateLimitExceeded" || reason=="userRateLimitExceeded")) ;
}

// The wait before retry number attempt (1 to 4): 500 ms, then 1 s, 2 s and 4 s, with a random fraction added, up to 4 s
static int rateLimitWait(int attempt)
{
    int base = 500 << (attempt-1) ;
    int wait = base + QRandomGenerator::global()->bounded(base/2 + 1) ;
    return qMin(wait, 4000) ;
}

// The status and reason, as the report gives them
static QString failureText(int status, const QString &reason, const QString &message)
{
    QString text = QString("Google answered HTTP %1").arg(status) ;
    if (!reason.isEmpty()) text += QString(" %1").arg(reason) ;
    if (!message.isEmpty()) text += QString(": %1").arg(message) ;
    return text ;
}

//=====================================================================================================
//
// Public: getNetworkError
//
// Return the last network error string, or "" if OK
//
QString GoogleAccess::getNetworkError()
{
    return errorstatus ;
}

int GoogleAccess::getNetworkErrorCode()
{
    return errorcode ;
}

bool GoogleAccess::isConnectionError()
{
    return connectionerror ;
}

// xmlpost - Submit an xml request

QString GoogleAccess::googlePutPostDelete(QString link, enum googleAction action, QString data, QString logfile, QString ifmatch)
{
    googlePutPostResponse="" ;
    errorcode=0 ;
    errorstatus="" ;
    errorrecovery="" ;
    // The pacing set after Google's rate limits, before each upload (issue #94)
    int pacingdelay = pacer.delayBeforeNext() ;
    if (pacingdelay>0) QThread::msleep(pacingdelay) ;
    bool hadratelimit=false ;
    int ratelimitretries=0 ;
    int ratelimitwait=0 ;
    QString logfilename = QString("%1").arg(logsequencenumber++,4,10,QChar('0')) + QString("-") + logfile + QString(".cmlog") ;
    int retries=1 ;
    int complete=false ;
    int readsuccess=false ;
    bool timeoutretry=false ;

    if (gConf->debugGoogleEnabled())
        writeToFile(gConf->getDatabasePath() + "/" + logfilename, "") ;

    do {

        if (accesstoken.isEmpty()) {

            readsuccess=false ;

        } else {

            QNetworkAccessManager &manager = netmanager ;
            QNetworkReply *reply = NULL ;
            QString QueryChar="?" ;
            if (link.contains("?")) QueryChar="&" ;
            QUrl url(link + QueryChar + "access_token=" + accesstoken) ;
            QNetworkRequest request(url) ;
            QEventLoop eventLoop ;

            // Starting flag state
            readsuccess=false ;
            timeoutretry=false ;
            complete=false ;

            // get the page
            QObject::connect(&manager, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));
            QByteArray submitdata = data.toStdString().c_str() ;

            QString header ;
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            header = "Content-Type: application/json\n" ;

            request.setHeader(QNetworkRequest::ContentLengthHeader, submitdata.length());
            header = header + "Content-Length: " + QString::number(submitdata.length()) + "\n" ;

            request.setRawHeader("charset", "UTF-8") ;
            header = header + "charset: UTF-8\n" ;

            QString auth = QString("Bearer ") + accesstoken ;
            request.setRawHeader("Authorization", auth.toLatin1()) ;
            header = header + "Authorization: Bearer " + accesstoken + "\n" ;

            // The ETag the change is made against, or "*" for any version (issue #86)
            request.setRawHeader("If-Match", ifmatch.toLatin1()) ;
            header = header + "If-Match: " + ifmatch + "\n" ;

            QString actionname ;

            // TIMING: request time, from sending it to the reply arriving
            QElapsedTimer requesttimer ;
            requesttimer.start() ;

            // A test fault answers this request instead of sending it, so Google does not see it (see testFaultPlan)
            const TestFault *fault = testFaultFor('w', ++writerequestcount) ;
            actionname = action==GoogleAccess::Post ? "POST" : action==GoogleAccess::Patch ? "PATCH" : action==GoogleAccess::Put ? "PUT" : "DELETE" ;
            if (!fault) switch (action) {
            case GoogleAccess::Post:
                reply = manager.post(request, submitdata) ;
                actionname="POST" ;
                break ;
            case GoogleAccess::Patch:
                reply = manager.sendCustomRequest(request, "PATCH", submitdata);
                actionname="PATCH" ;
                break ;
            case GoogleAccess::Put:
                reply = manager.put(request, submitdata) ;
                actionname="PUT" ;
                break ;
            case GoogleAccess::Delete:
                // From PHP - $req->setRequestHeaders(array('content-type' => 'application/atom+xml; charset=UTF-8; type=feed'));
                // From stackexchange: "GData-Version": "3.0", "Authorization":"Bearer " + token.accesstoken, "if-match":"*"
                reply = manager.deleteResource(request) ;
                actionname="DELETE" ;
                break ;
            }

            if (gConf->debugGoogleEnabled())
                writeToFile(gConf->getDatabasePath() + "/" + logfilename,
                            QString("URL: ") + link + QueryChar + "access_token=" + accesstoken +
                            QString("\n\nHEADER >>\n") + header +
                            QString("\n\n") + actionname + QString(">>\n") + data +
                            QString("\n\n"), true) ;

            if (!fault) eventLoop.exec() ;
            QVariant replycode = fault ? QVariant(fault->status) : reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) ;
            errorcode=replycode.toInt() ;
            // logfile can contain a contact name, so only its first part is printed
            dbg(QString("TIMING http %1 %2 status=%3 ms=%4").arg(actionname).arg(logfile.section('-',0,0)).arg(errorcode).arg(requesttimer.elapsed())) ;

            // The body is read once, for the result and the reason Google gives (issue #94)
            QByteArray body = replyBody(reply, fault) ;
            QString reason, message ;
            googleReason(body, reason, message) ;
            int status = replycode.toInt() ;

            // if page load is OK, get details, else set error string
            switch (fault ? (fault->status<0 ? QNetworkReply::RemoteHostClosedError : QNetworkReply::NoError) : reply->error()) {
            case QNetworkReply::ConnectionRefusedError:
                connectionerror = true ;
                errorstatus="Connection Refused." ;
                errorrecovery="Could not reach Google. Sync again: the sync resumes from its progress file, so uploads already done are not repeated." ;
                break ;
            case QNetworkReply::RemoteHostClosedError:
                connectionerror = true ;
                errorstatus="Connection closed by Google (Remote Host Closed Connection)." ;
                errorrecovery="Google closed the connection. Sync again: the sync resumes from its progress file, so uploads already done are not repeated." ;
                break ;
            case QNetworkReply::HostNotFoundError:
                connectionerror = true ;
                errorstatus="Host accounts.google.com Not Found." ;
                errorrecovery="Could not find Google. Check the connection, then sync again." ;
                break ;
            case QNetworkReply::UnknownServerError:
                connectionerror = true ;
                errorstatus="Unknown Server Error." ;
                errorrecovery="Google reported a server error. Sync again: the sync resumes from its progress file." ;
                break ;
            default:

                connectionerror = false ;
                if (status>=200 && status<=299) {
                  errorstatus = "" ;
                  googlePutPostResponse = body ;
                  readsuccess=true ;

                } else if (status==412) {
                  // Precondition Failed: Google's copy has changed since the ETag sent.  The caller decides what to do with it,
                  // so it is not retried here (issue #86)
                  errorstatus = "Network Error 412" ;
                  googlePutPostResponse = body ;
                  readsuccess=true ;

                } else if (isRateLimit(status, reason) && ratelimitretries<4) {
                  // Google's rate limit: wait, then ask again.  Four retries, then the request is a failure (issue #94)
                  ratelimitretries++ ;
                  ratelimitwait = rateLimitWait(ratelimitretries) ;
                  hadratelimit = true ;
                  dbg(QString("TIMING http rate limited %1 - %2, waiting %3 ms before retry %4 of 4").arg(logfile.section('-',0,0))
                      .arg(failureText(status, reason, message)).arg(ratelimitwait).arg(ratelimitretries)) ;
                  timeoutretry = true ;

                } else if (isRateLimit(status, reason)) {
                  googlePutPostResponse = body ;
                  hadratelimit = true ;
                  errorstatus = failureText(status, reason, message) + " (still limited after 4 retries)" ;
                  errorrecovery = "Google's rate limit did not clear. Wait five minutes, then sync again: the uploads already done are not repeated." ;
                  readsuccess=true ;

                } else if (status==0) {
                  // No answer from Google at all: tried once more, as before, and then reported
                  errorstatus = "Network Error " + reply->errorString() ;
                  errorrecovery = "Could not reach Google. Sync again: the sync resumes from its progress file, so uploads already done are not repeated." ;
                  connectionerror = true ;
                  readsuccess=false ;

                } else if (status==401) {
                  // Not signed in: sign in again once, then report it (the sign-in is refreshed below)
                  errorstatus = failureText(status, reason, message) ;
                  errorrecovery = "Sign in to Google again, then sync." ;
                  readsuccess=false ;

                } else {
                  // Anything else is not retried.  It is reported with Google's reason, so the item can be put right (issue #94)
                  googlePutPostResponse = body ;
                  errorstatus = failureText(status, reason, message) ;
                  errorrecovery = "Correct the item named above, then sync again." ;
                  readsuccess=true ;
                }
            }
            // The reply has been read, so free it (the manager is shared)
            if (reply) reply->deleteLater() ;
        }

        if (timeoutretry==true) {
            // A rate limit: wait, then try again (issue #94)
            QThread::msleep(ratelimitwait) ;
            complete=false ;

        } else if (readsuccess || retries==0) {
            // Complete on successful read
            complete=true ;

        } else {
            // If there is an error refresh the access token and retry once
            googleGetAccessToken() ;
            retries-- ;
        }

        if (gConf->debugGoogleEnabled())
            writeToFile(gConf->getDatabasePath() + "/" + logfilename,
                        QString("RESPONSE>>\n") + googlePutPostResponse +
                        QString("\n\nSTATUS>>\n") + QString::number(errorcode) + QString(" ") + errorstatus +
                        QString("\n\n--------------------------\n\n"), true) ;

    } while (!complete) ;

    // The pacing counts each transaction, and whether it needed a retry (issue #94)
    pacer.finished(hadratelimit) ;
    if (!errorstatus.isEmpty()) {
        lastfailure = errorstatus ;
        lastrecovery = errorrecovery ;
    }
    return googlePutPostResponse ;
}

//*****************************************************************************************************

// get - http get a web page, and return the resulting string

QString GoogleAccess::googleGet(QString link, QString logfile)
{
    int retries=1 ;
    int complete=false ;
    int readsuccess=false ;
    QString logseq = QString("%1").arg(logsequencenumber++,4,10,QChar('0')) ;
    QString logfilename = logseq + QString("-") + logfile + QString(".cmlog") ;

    errorcode=0 ;
    errorstatus="" ;
    errorrecovery="" ;
    googleGetResponse="" ;
    // Rate limits are retried as for writes (issue #94)
    int ratelimitretries=0 ;
    int ratelimitwait=0 ;

    if (gConf->debugGoogleEnabled())
        writeToFile(gConf->getDatabasePath() + "/" + logfilename, "") ;

    do {
        if (accesstoken.isEmpty()) {

            googleGetResponse = "" ;
            readsuccess=false ;

        } else {

            QString QueryChar="?" ;
            if (link.contains("?")) QueryChar="&" ;
            QNetworkAccessManager &manager = netmanager ;
            QNetworkReply *reply = NULL ;
            QUrl url(link + QueryChar + "access_token=" + accesstoken) ;
            QNetworkRequest request(url) ;
            QEventLoop eventLoop ;

            if (gConf->debugGoogleEnabled())
                writeToFile(gConf->getDatabasePath() + "/" + logfilename,
                            QString("URL: ") + link + QueryChar + "access_token=" + accesstoken +
                            QString("\n\nGET>>") +
                            QString("\n\n"), true) ;

            // get the page
            QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
            // TIMING: request time, from sending it to the reply arriving
            QElapsedTimer requesttimer ;
            requesttimer.start() ;
            // A test fault answers this request instead of sending it, so Google does not see it (see testFaultPlan)
            const TestFault *fault = testFaultFor('r', ++readrequestcount) ;
            if (!fault) {
                reply = manager.get(request);
                eventLoop.exec() ;
            }
            QVariant replycode = fault ? QVariant(fault->status) : reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) ;
            errorcode=replycode.toInt() ;
            dbg(QString("TIMING http GET %1 status=%2 ms=%3").arg(logfile.section('-',0,0)).arg(errorcode).arg(requesttimer.elapsed())) ;

            switch (fault ? (fault->status<0 ? QNetworkReply::RemoteHostClosedError : QNetworkReply::NoError) : reply->error()) {
            case QNetworkReply::ConnectionRefusedError:
                connectionerror=true ;
                errorstatus="Connection Refused" ;
                break ;
            case QNetworkReply::RemoteHostClosedError:
                connectionerror=true ;
                errorstatus="Remote Host Closed Connection" ;
                break ;
            case QNetworkReply::HostNotFoundError:
                connectionerror=true ;
                errorstatus="Host accounts.google.com Not Found" ;
                break ;
            case QNetworkReply::UnknownServerError:
                connectionerror=true ;
                errorstatus="Unknown Server Error" ;
                break ;
            default:
                connectionerror=false ;
                // if page load is OK, get details, else set error string
                // The body gives Google's reason for a failure (issue #94)
                QByteArray body = replyBody(reply, fault) ;
                QString reason, message ;
                googleReason(body, reason, message) ;
                int status = replycode.toInt() ;
                if (status>=200 && status<=299) {
                  googleGetResponse = body ;
                  errorstatus="" ;
                  readsuccess=true ;
                } else if (isRateLimit(status, reason) && ratelimitretries<4) {
                  // Google's rate limit: wait, then ask again (issue #94)
                  ratelimitretries++ ;
                  ratelimitwait = rateLimitWait(ratelimitretries) ;
                  errorstatus = failureText(status, reason, message) ;
                  readsuccess=false ;
                } else if (isRateLimit(status, reason)) {
                  googleGetResponse = "" ;
                  errorstatus = failureText(status, reason, message) + " (still limited after 4 retries)" ;
                  errorrecovery = "Google's rate limit did not clear. Wait five minutes, then sync again." ;
                  readsuccess=true ;
                } else if (status==401) {
                  // Not signed in: the sign-in is refreshed once below
                  googleGetResponse = "" ;
                  errorstatus = failureText(status, reason, message) ;
                  errorrecovery = "Sign in to Google again, then sync." ;
                  readsuccess=false ;
                } else if (status==0) {
                  // No answer from Google: tried once more, as before
                  googleGetResponse = "" ;
                  errorstatus = "Network Error " + reply->errorString() ;
                  errorrecovery = "Could not reach Google. Sync again." ;
                  readsuccess=false ;
                } else {
                  // Anything else is not retried: it is reported with Google's reason (issue #94)
                  googleGetResponse = body ;
                  errorstatus = failureText(status, reason, message) ;
                  errorrecovery = "Sync again once the cause in the report is put right." ;
                  readsuccess=true ;
                }
                break ;
            }
            // The reply has been read, so free it (the manager is shared)
            if (reply) reply->deleteLater() ;
        }

        // Add log file name to error status
        if (!errorstatus.isEmpty() && gConf->debugGoogleEnabled()) {
            errorstatus = logseq + QString(": ") + errorstatus ;
        }

        // A rate limit is waited out before the request is sent again (issue #94).  Otherwise an error refreshes the access
        // token and retries once.
        if (ratelimitwait>0) {
            QThread::msleep(ratelimitwait) ;
            ratelimitwait=0 ;
            complete=false ;
        } else if (readsuccess || retries==0) {
            complete=true ;
        } else {
            googleGetAccessToken() ;
            retries-- ;
       }

        if (gConf->debugGoogleEnabled())
            writeToFile(gConf->getDatabasePath() + "/" + logfilename,
                        QString("RESPONSE>>\n") + googleGetResponse +
                        QString("\n\nSTATUS>>\n") + QString::number(errorcode) + QString(" ") + errorstatus +
                        QString("\n\n--------------------------\n\n"), true) ;


    } while (!complete) ;


    if (!errorstatus.isEmpty()) {
        lastfailure = errorstatus ;
        lastrecovery = errorrecovery ;
    }
    return googleGetResponse ;
}
