#include "sms.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

// The agent is the name given in the ini file ("voodoo").  A blank or unknown name is Unknown, and send and getBalance
// then return BadAgent.
SMS::SMS(QString& agent, QString& username, QString& password)
{
    this->password = password ;
    this->username = username ;

    QString name = agent.trimmed().toLower() ;
    if (name.compare("voodoo")==0) this->agent=Voodoo ;
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
//          -5 - Bad Agent
//           1 - Send OK

int SMS::send(QString number, QString from, QString message)
{
    switch (agent) {
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
    case Voodoo:
        return getVoodooBalance() ;
    case Unknown:
        break ;
    }
    return BadAgent ;
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
    // unit it is in.  It is taken as pounds here and converted to pennies.
    // CHECK this against the account before relying on the low-credit warning.
    QRegularExpression credit("<credit>\\s*([0-9]+(\\.[0-9]+)?)\\s*</credit>") ;
    QRegularExpressionMatch creditmatch = credit.match(QString::fromUtf8(reply->readAll())) ;
    if (creditmatch.hasMatch()) {
        return qRound(creditmatch.captured(1).toDouble() * 100) ;
    }

    // Unable to decode
    return -3 ;
}
