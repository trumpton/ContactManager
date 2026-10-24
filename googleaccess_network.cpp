
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

//*****************************************************************************************************


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

QString GoogleAccess::googlePutPostDelete(QString link, enum googleAction action, QString data, QString logfile)
{
    googlePutPostResponse="" ;
    errorcode=0 ;
    errorstatus="" ;
    QString logfilename = QString("%1").arg(logsequencenumber++,4,10,QChar('0')) + QString("-") + logfile + QString(".cmlog") ;
    int retries=1 ;
    int complete=false ;
    int readsuccess=false ;
    int timeoutretries=5 ;
    bool timeoutretry=false ;
    int timeoutscalefactor=1 ;

    if (gConf->debugGoogleEnabled())
        writeToFile(gConf->getDatabasePath() + "/" + logfilename, "") ;

    do {

        if (accesstoken.isEmpty()) {

            readsuccess=false ;

        } else {

            QNetworkAccessManager manager ;
            QNetworkReply *reply ;
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

            request.setRawHeader("If-Match", "*") ;
            header = header + "If-Match: *\n" ;

            QString actionname ;

            // TIMING: request time, from sending it to the reply arriving
            QElapsedTimer requesttimer ;
            requesttimer.start() ;

            switch (action) {
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

            eventLoop.exec() ;
            QVariant replycode=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) ;
            errorcode=replycode.toInt() ;
            // logfile can contain a contact name, so only its first part is printed
            dbg(QString("TIMING http %1 %2 status=%3 ms=%4").arg(actionname).arg(logfile.section('-',0,0)).arg(errorcode).arg(requesttimer.elapsed())) ;

            // TODO: For some reason, google does not update the "updated"
            //       this means that we will keep uploading the same records
            //       again and again

            // if page load is OK, get details, else set error string
            switch (reply->error()) {
            case QNetworkReply::ConnectionRefusedError:
                connectionerror = true ;
                errorstatus="Connection Refused." ;
                break ;
            case QNetworkReply::RemoteHostClosedError:
                connectionerror = true ;
                errorstatus="Remote Host Closed Connection." ;
                break ;
            case QNetworkReply::HostNotFoundError:
                connectionerror = true ;
                errorstatus="Host accounts.google.com Not Found." ;
                break ;
            case QNetworkReply::UnknownServerError:
                connectionerror = true ;
                errorstatus="Unknown Server Error." ;
                break ;
            default:

                connectionerror = false ;
                if (replycode.toInt()>=200 && replycode.toInt()<=299) {
                  errorstatus = "" ;
                  googlePutPostResponse = reply->readAll() ;
                  readsuccess=true ;

                } else if (replycode.toInt()==429) {
                    if (timeoutretries>0) {
                        timeoutretries-- ;
                        timeoutscalefactor*=2 ;
                        timeoutretry=true ;
                    } else {
                        googlePutPostResponse = reply->readAll() ;
                        errorstatus="Google write quota limit exceeded" ;
                        readsuccess=true ;
                        timeoutretry=false ;
                    }

                } else {
                  errorstatus = "Network Error " + replycode.toString() ;
                  googlePutPostResponse = reply->readAll() ;
                  connectionerror=true ;
                  readsuccess=false ;
                }
            }
        }

        if (timeoutretry==true) {
            // if there is a timeout error, try again
            dbg(QString("TIMING http backoff %1 - waiting %2 ms before retry").arg(logfile.section('-',0,0)).arg(800*timeoutscalefactor)) ;
            QThread::msleep(800*timeoutscalefactor) ;
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

    return googlePutPostResponse ;
}

//*****************************************************************************************************

// TODO: applies to all network connections - if the computer does not have an active network
//       interface, the app segfaults

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
    googleGetResponse="" ;

    if (gConf->debugGoogleEnabled())
        writeToFile(gConf->getDatabasePath() + "/" + logfilename, "") ;

    do {
        if (accesstoken.isEmpty()) {

            googleGetResponse = "" ;
            readsuccess=false ;

        } else {

            QString QueryChar="?" ;
            if (link.contains("?")) QueryChar="&" ;
            QNetworkAccessManager manager ;
            QNetworkReply *reply ;
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
            reply = manager.get(request);
            eventLoop.exec() ;
            QVariant replycode=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) ;
            errorcode=replycode.toInt() ;
            dbg(QString("TIMING http GET %1 status=%2 ms=%3").arg(logfile.section('-',0,0)).arg(errorcode).arg(requesttimer.elapsed())) ;

            switch (reply->error()) {
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
                if (replycode.toInt()>=200 && replycode.toInt()<=299) {
                  googleGetResponse = reply->readAll() ;
                  errorstatus="" ;
                  readsuccess=true ;
                } else {
                  googleGetResponse = "" ;
                  errorstatus = "Network Error " + replycode.toString() ;
                  googleGetResponse = reply->readAll() ;
                  readsuccess=false ;
                }
                break ;
            }
        }

        // Add log file name to error status
        if (!errorstatus.isEmpty() && gConf->debugGoogleEnabled()) {
            errorstatus = logseq + QString(": ") + errorstatus ;
        }

        // if there is an error refresh the access token and retry once
        if (readsuccess || retries==0) {
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


    return googleGetResponse ;
}
