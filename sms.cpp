#include "sms.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

// The agent is the name given in the ini file.  TextAnywhere is the name Clockwork SMS now uses, so it is an alias for
// Clockwork.  A blank or unknown name is Unknown, and send and getBalance then return BadAgent.
SMS::SMS(QString& agent, QString& username, QString& password)
{
    this->password = password ;
    this->username = username ;

    QString name = agent.trimmed().toLower() ;
    if (name.compare("clockwork")==0 || name.compare("textanywhere")==0) this->agent=Clockwork ;
    else if (name.compare("voodoo")==0) this->agent=Voodoo ;
    else {
        this->agent=Unknown ;
        smsErrorData = "Unknown SMS agent: '" + agent + "'" ;
    }
}

bool SMS::checkCredits()
{
    if (smsErrorData.left(8).toLower().compare("error 3:")==0) return false ;
    else return true ;
}

QString& SMS::getErrorMessage()
{
    return smsErrorData ;
}

// Returns: -1 - Not Started Sending
//          -2 - Network Error
//          -3 - Error during transaction
//          -4 - Unable to Send / Possibly Sent
//           1 - Send OK

int SMS::send(QString number, QString from, QString message)
{
    switch (agent) {
    case Clockwork:
        return sendClockwork(number, from, message) ;
    case Voodoo:
        return sendVoodoo(number, from, message) ;
    case Unknown:
        break ;
    }
    return BadAgent ;
}

// The balance is in pennies: 1250 is £12.50
int SMS::getBalance()
{
    switch (agent) {
    case Clockwork:
        return getClockworkBalance() ;
    case Voodoo:
        return getVoodooBalance() ;
    case Unknown:
        break ;
    }
    return BadAgent ;
}


//////////////////////////////////////////////////////////////
//
// Clockwork SMS API
//

int SMS::sendClockwork(QString number, QString from, QString message)
{
    QString URL = "https://api.clockworksms.com/http/send.aspx" ;

    QUrl url(URL) ;
    QNetworkRequest request(url) ;
    QNetworkAccessManager manager ;
    QEventLoop eventLoop ;
    QNetworkReply *reply ;
    QByteArray params ;

    number = number.replace(" ", "") ;
    from = from.replace(" ", "") ;
    message = message.replace(" ", "+") ;

    params.append("key=").append(password.toLatin1()).append("&to=").append(number.toLatin1()).append("&content=").append(message.toLatin1()) ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    qDebug() << "SMS Connecting to " << URL ;

    /*
    // This is not supported in QT6
    QNetworkAccessManager::NetworkAccessibility na = manager.networkAccessible() ;
    if (na != QNetworkAccessManager::Accessible) {
        // Connection Failure (network not up)
        qDebug() << "Connection Failed" ;
        return -2 ;
    }
    */

    qDebug() << "Sending " << params ;
    reply = manager.post(request, params) ;
    eventLoop.exec() ;

    if (reply->error()!=QNetworkReply::NoError) {
        // Unable to send
        qDebug() << "Post returned error " << reply->errorString() ;
        return -1 ;
    }

    smsErrorData = reply->readAll().trimmed() ;
    bool iserror = smsErrorData.left(5).toLower().compare("error")==0 ;

    if (iserror) {
        qDebug() << "Post response error " << smsErrorData ;
        // Received error response
        return -3 ;
    } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)==200) {
        qDebug() << "Post returned 200 OK" ;
        // Received 200 OK response
        return 1 ;
    } else {
        // Not received 200 OK response
        return -4 ;
    }

}

int SMS::getClockworkBalance()
{
    QString URL = "https://api.clockworksms.com/http/balance" ;

    QUrl url(URL) ;
    QNetworkRequest request(url) ;
    QNetworkAccessManager manager ;
    QEventLoop eventLoop ;
    QNetworkReply *reply ;
    QByteArray params ;

    params.append("key=").append(password.toLatin1()) ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    /*
    // Not supported in QT6
    if (manager.networkAccessible() == QNetworkAccessManager::NotAccessible) {
        // Connection Error
        return -2 ;
    }
    */

    reply = manager.post(request, params) ;
    eventLoop.exec() ;

    // Error response returned
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)!=200) return -4 ;

    QString balance = reply->readAll().toLower().trimmed() ;
    QRegularExpression exp("[^0-9]*([0-9]+)[\\.:]([0-9]+)[^0-9]*") ;
    QRegularExpressionMatch expmatch = exp.match(balance) ;
    if (expmatch.hasMatch()) {
        // The pence are the two digits after the point, so "12.5" is £12.50, not £12.05
        int pounds = expmatch.captured(1).toInt() ;
        int pence = expmatch.captured(2).leftJustified(2, '0').left(2).toInt() ;
        return pounds * 100 + pence ;
    }

    // Unable to decode
    return -3 ;
}

//////////////////////////////////////////////////////////////
//
// Voodoo SMS API
//
// The username and password are the HTTP API user's, from the VoodooSMS portal (uid and pass).

int SMS::sendVoodoo(QString number, QString from, QString message)
{
    QString URL = "https://www.voodoosms.com/vapi/server/sendSMS" ;

    // The sender is letters and digits only, and at most 11 of them
    from = from.remove(QRegularExpression("[^A-Za-z0-9]")).left(11) ;
    number = number.remove(' ') ;

    QUrlQuery query ;
    query.addQueryItem("uid", username) ;
    query.addQueryItem("pass", password) ;
    query.addQueryItem("dest", number) ;
    query.addQueryItem("orig", from) ;
    query.addQueryItem("msg", message) ;
    query.addQueryItem("validity", "1") ;
    query.addQueryItem("format", "XML") ;

    QUrl url(URL) ;
    QNetworkRequest request(url) ;
    QNetworkAccessManager manager ;
    QEventLoop eventLoop ;
    QNetworkReply *reply ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    qDebug() << "SMS Connecting to " << URL ;

    // The parameters are not logged, as they include the password
    reply = manager.post(request, query.query(QUrl::FullyEncoded).toUtf8()) ;
    eventLoop.exec() ;

    if (reply->error()!=QNetworkReply::NoError) {
        // Unable to send
        smsErrorData = reply->errorString() ;
        return -1 ;
    }

    smsErrorData = reply->readAll().trimmed() ;
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ;

    // The XML response gives the result as <result>200</result> when the message is accepted
    QRegularExpression result("<result>\\s*([0-9]+)") ;
    QRegularExpressionMatch resultmatch = result.match(smsErrorData) ;
    if (status==200 && resultmatch.hasMatch() && resultmatch.captured(1)=="200") {
        return 1 ;
    }

    // The service answered with an error (including 401 Unauthorized)
    if (status==200 || status==401) return -3 ;

    // Not received 200 OK response
    return -4 ;
}

int SMS::getVoodooBalance()
{
    QUrl url("https://www.voodoosms.com/vapi/server/getCredit") ;
    QUrlQuery query ;
    query.addQueryItem("uid", username) ;
    query.addQueryItem("pass", password) ;
    query.addQueryItem("format", "XML") ;
    url.setQuery(query) ;

    QNetworkRequest request(url) ;
    QNetworkAccessManager manager ;
    QEventLoop eventLoop ;
    QNetworkReply *reply ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));

    reply = manager.get(request) ;
    eventLoop.exec() ;

    // Error response returned
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)!=200) return -4 ;

    // The balance is in <credit>, as a decimal, e.g. <credit>9928597.0680</credit>.  The API page does not say what
    // unit it is in.  It is taken as pounds here, so that it can be compared with the pennies figure from Clockwork.
    // CHECK this against the account before relying on the low-credit warning.
    QRegularExpression credit("<credit>\\s*([0-9]+(\\.[0-9]+)?)\\s*</credit>") ;
    QRegularExpressionMatch creditmatch = credit.match(QString::fromUtf8(reply->readAll())) ;
    if (creditmatch.hasMatch()) {
        return qRound(creditmatch.captured(1).toDouble() * 100) ;
    }

    // Unable to decode
    return -3 ;
}

