#include "sms.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

// The longest a request to the SMS service may take.  Without this, a service that stops replying would leave the app
// waiting for ever.
static const int SMS_TIMEOUT_MS = 30000 ;

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

QString SMS::internationalNumber(QString number, QString countrycode)
{
    // Only digits and a leading + are kept, so spaces, dashes and brackets are dropped
    QString digits = number.remove(QRegularExpression("[^0-9+]")) ;

    if (digits.startsWith("+")) return digits.mid(1) ;
    if (digits.startsWith("00")) return digits.mid(2) ;

    // A national number: the country code replaces the leading 0 (the country code may be written 0044)
    QString country = countrycode.remove(QRegularExpression("[^0-9]")) ;
    if (digits.startsWith("0") && !country.isEmpty()) {
        if (country.startsWith("00")) country = country.mid(2) ;
        return country + digits.mid(1) ;
    }

    return digits ;
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

    // Checked here, so that the reason is reported rather than the service's refusal of it
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        smsErrorData = "The SMS username or password is blank. Set the username in the [sms-reminder] section of ContactManager.ini, and the SMS password in Settings." ;
        return -1 ;
    }
    if (number.isEmpty()) {
        smsErrorData = "There is no mobile number to send to." ;
        return -1 ;
    }
    if (from.length()<3) {
        smsErrorData = "The sender needs to be 3 to 11 letters or digits, and there is none for this message." ;
        return -1 ;
    }

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
    request.setTransferTimeout(SMS_TIMEOUT_MS) ;

    qDebug() << "SMS Connecting to " << URL ;

    // The parameters are not logged, as they include the password
    reply = manager.post(request, query.query(QUrl::FullyEncoded).toUtf8()) ;
    eventLoop.exec() ;

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ;
    QString body = QString::fromUtf8(reply->readAll()).trimmed() ;

    if (reply->error()!=QNetworkReply::NoError) {

        // The service refused the login (Qt reports a 401 as an authentication error)
        if (status==401 || reply->error()==QNetworkReply::AuthenticationRequiredError) {
            smsErrorData = "Voodoo rejected the SMS username or password (HTTP 401). Check them in ContactManager.ini and Settings." ;
            return -3 ;
        }

        // A reply that timed out, or a connection that dropped, may still have been accepted by the service, so the
        // message is not sent again.  Otherwise the message was not sent.
        if (reply->error()==QNetworkReply::OperationCanceledError || reply->error()==QNetworkReply::RemoteHostClosedError) {
            smsErrorData = "No reply from Voodoo in time, so the message may have been sent." ;
            return -4 ;
        }

        smsErrorData = "Could not reach Voodoo: " + reply->errorString() ;
        if (!body.isEmpty()) smsErrorData += " (" + body + ")" ;
        return -1 ;
    }

    smsErrorData = body ;

    // The XML response gives the result as <result>200</result> when the message is accepted
    QRegularExpression result("<result>\\s*([0-9]+)") ;
    QRegularExpressionMatch resultmatch = result.match(body) ;
    if (status==200 && resultmatch.hasMatch() && resultmatch.captured(1)=="200") {
        return 1 ;
    }

    // The service answered, but did not accept the message
    if (status==200) {
        smsErrorData = "Voodoo did not accept the message: " + body ;
        return -3 ;
    }

    // Not received 200 OK response
    smsErrorData = QString("Voodoo replied HTTP %1: %2").arg(status).arg(body) ;
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
    request.setTransferTimeout(SMS_TIMEOUT_MS) ;

    reply = manager.get(request) ;
    eventLoop.exec() ;

    // The reason for a failure is left in the error message, as for send
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ;
    QString body = QString::fromUtf8(reply->readAll()).trimmed() ;

    if (username.trimmed().isEmpty() || password.isEmpty()) {
        smsErrorData = "The SMS username or password is blank, so the balance cannot be read." ;
        return -2 ;
    }

    // No reply, or the request timed out
    if (reply->error()!=QNetworkReply::NoError) {
        if (status==401 || reply->error()==QNetworkReply::AuthenticationRequiredError) {
            smsErrorData = "Voodoo rejected the SMS username or password (HTTP 401) for the balance." ;
        } else {
            smsErrorData = "Could not reach Voodoo for the balance: " + reply->errorString() ;
        }
        return -2 ;
    }

    // Error response returned
    if (status!=200) {
        smsErrorData = QString("Voodoo replied HTTP %1 for the balance: %2").arg(status).arg(body) ;
        return -4 ;
    }

    // The balance is in <credit>, as a decimal, e.g. <credit>9928597.0680</credit>.  The API page does not say what
    // unit it is in.  It is taken as pounds here and converted to pennies.
    // CHECK this against the account before relying on the low-credit warning.
    QRegularExpression credit("<credit>\\s*([0-9]+(\\.[0-9]+)?)\\s*</credit>") ;
    QRegularExpressionMatch creditmatch = credit.match(body) ;
    if (creditmatch.hasMatch()) {
        return qRound(creditmatch.captured(1).toDouble() * 100) ;
    }

    // Unable to decode
    smsErrorData = "Could not read the balance from the reply: " + body ;
    return -3 ;
}
