#include "sms.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

// The longest a request to the SMS service may take.  Without this, a service that stops replying would leave the app
// waiting for ever.
static const int SMS_TIMEOUT_MS = 30000 ;

// The login as it is written to a debug file: only its first 5 characters are kept
static QString maskedLogin(const QString &value)
{
    return value.left(5) + QString(qMax(0, value.length()-5), QChar('*')) ;
}

// A request as it is written to a debug file.  The uid is written whole; the pass is masked.
static QString loggedRequest(const QString &method, const QString &url, const QUrlQuery &query)
{
    QString text = method + " " + url + "\n" ;
    for (const auto &item : query.queryItems()) {
        QString value = item.second ;
        if (item.first=="pass") value = maskedLogin(value) ;
        text += item.first + "=" + value + "\n" ;
    }
    return text ;
}

// Debug: each transaction with the SMS service is written to a file in the database folder, numbered like the Google
// debug files (NNNN-name.smslog), when ini [sms-reminder] debug=yes.
static void logSMSTransaction(const QString &name, const QString &request, int status, const QString &response,
                              const QString &error, int code)
{
    if (!gConf || !gConf->debugSMSEnabled()) return ;

    // The next number follows the highest one already there
    QDir logdir(gConf->getDatabasePath()) ;
    int next = 1 ;
    for (const QString &file : logdir.entryList(QStringList() << "????-*.smslog", QDir::Files)) {
        next = qMax(next, file.left(4).toInt()+1) ;
    }

    QString text = "REQUEST>>\n" + request + "\n"
                 + "STATUS>>\nHTTP " + QString::number(status) + "\n"
                 + "Error: " + error + "\n"
                 + "Result: " + QString::number(code) + "\n\n"
                 + "RESPONSE>>\n" + response + "\n" ;

    QString filename = QString("%1").arg(next, 4, 10, QChar('0')) + "-" + name + ".smslog" ;
    writeToFile(gConf->getDatabasePath() + "/" + filename, text) ;
}

// The agent is the name given in the ini file ("voodoo").  A blank or unknown name is Unknown, and send and getBalance
// then return BadAgent.
SMS::SMS(QString& agent, QString& username, QString& password)
{
    this->password = password ;
    this->username = username ;

    QString name = agent.trimmed().toLower() ;
    if (name.compare("voodoo")==0) this->agent=Voodoo ;
    else if (name.compare("esendex")==0) this->agent=Esendex ;
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
    case Esendex:
        return sendEsendex(number, from, message) ;
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
    case Esendex:
        return getEsendexBalance() ;
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

    // The parameters are not logged here, as they include the password
    reply = manager.post(request, query.query(QUrl::FullyEncoded).toUtf8()) ;
    eventLoop.exec() ;

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ;
    QString body = QString::fromUtf8(reply->readAll()).trimmed() ;
    int code ;

    if (reply->error()!=QNetworkReply::NoError) {

        // The service refused the login (Qt reports a 401 as an authentication error)
        if (status==401 || reply->error()==QNetworkReply::AuthenticationRequiredError) {
            smsErrorData = "Voodoo rejected the SMS username or password (HTTP 401). Check them in ContactManager.ini and Settings." ;
            if (!body.isEmpty()) smsErrorData += " Voodoo said: " + body ;
            code = -3 ;
        }

        // A reply that timed out, or a connection that dropped, may still have been accepted by the service, so the
        // message is not sent again.  Otherwise the message was not sent.
        else if (reply->error()==QNetworkReply::OperationCanceledError || reply->error()==QNetworkReply::RemoteHostClosedError) {
            smsErrorData = "No reply from Voodoo in time, so the message may have been sent." ;
            code = -4 ;
        }

        else {
            smsErrorData = "Could not reach Voodoo: " + reply->errorString() ;
            if (!body.isEmpty()) smsErrorData += " (" + body + ")" ;
            code = -1 ;
        }

    } else {

        smsErrorData = body ;

        // The XML response gives the result as <result>200</result> when the message is accepted
        QRegularExpression result("<result>\\s*([0-9]+)") ;
        QRegularExpressionMatch resultmatch = result.match(body) ;

        if (status==200 && resultmatch.hasMatch() && resultmatch.captured(1)=="200") {
            code = 1 ;
        } else if (status==200) {
            // The service answered, but did not accept the message
            smsErrorData = "Voodoo did not accept the message: " + body ;
            code = -3 ;
        } else {
            // Not received 200 OK response
            smsErrorData = QString("Voodoo replied HTTP %1: %2").arg(status).arg(body) ;
            code = -4 ;
        }
    }

    logSMSTransaction("send", loggedRequest("POST", URL, query), status, body, reply->errorString(), code) ;
    return code ;
}

int SMS::getVoodooBalance()
{
    QString URL = "https://www.voodoosms.com/vapi/server/getCredit" ;

    if (username.trimmed().isEmpty() || password.isEmpty()) {
        smsErrorData = "The SMS username or password is blank, so the balance cannot be read." ;
        return -2 ;
    }

    QUrlQuery query ;
    query.addQueryItem("uid", username) ;
    query.addQueryItem("pass", password) ;
    query.addQueryItem("format", "XML") ;

    QUrl url(URL) ;
    url.setQuery(query) ;

    QNetworkRequest request(url) ;
    QNetworkAccessManager manager ;
    QEventLoop eventLoop ;
    QNetworkReply *reply ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setTransferTimeout(SMS_TIMEOUT_MS) ;

    reply = manager.get(request) ;
    eventLoop.exec() ;

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ;
    QString body = QString::fromUtf8(reply->readAll()).trimmed() ;
    int code ;

    if (reply->error()!=QNetworkReply::NoError) {
        // No reply, the request timed out, or the login was refused
        if (status==401 || reply->error()==QNetworkReply::AuthenticationRequiredError) {
            smsErrorData = "Voodoo rejected the SMS username or password (HTTP 401) for the balance." ;
            if (!body.isEmpty()) smsErrorData += " Voodoo said: " + body ;
        } else {
            smsErrorData = "Could not reach Voodoo for the balance: " + reply->errorString() ;
        }
        code = -2 ;

    } else if (status!=200) {
        // Error response returned
        smsErrorData = QString("Voodoo replied HTTP %1 for the balance: %2").arg(status).arg(body) ;
        code = -4 ;

    } else {
        // The balance is in <credit>, as a decimal, e.g. <credit>9928597.0680</credit>.  The API page does not say what
        // unit it is in.  It is taken as pounds here and converted to pennies.
        // CHECK this against the account before relying on the low-credit warning.
        QRegularExpression credit("<credit>\\s*([0-9]+(\\.[0-9]+)?)\\s*</credit>") ;
        QRegularExpressionMatch creditmatch = credit.match(body) ;
        if (creditmatch.hasMatch()) {
            code = qRound(creditmatch.captured(1).toDouble() * 100) ;
        } else {
            // Unable to decode
            smsErrorData = "Could not read the balance from the reply: " + body ;
            code = -3 ;
        }
    }

    logSMSTransaction("balance", loggedRequest("GET", URL, query), status, body, reply->errorString(), code) ;
    return code ;
}

//////////////////////////////////////////////////////////////
//
// Esendex SMS API (the REST API at developer.esendex.co.uk)
//
// The username is the account reference (My Account in the Esendex portal).  The password, set in Settings, is the
// API key (Developer Tools in the portal).  Both are sent in headers, and the account reference in the balance request.

// A request as it is written to a debug file.  The account reference is written whole; the API key is masked.
static QString loggedEsendexRequest(const QString &method, const QString &url, const QString &accountref,
                                    const QString &apikey, const QByteArray &body)
{
    return method + " " + url + "\n"
         + "X-API-Key: " + maskedLogin(apikey) + "\n"
         + "accountReference: " + accountref + "\n"
         + "Content-Type: application/json\n\n"
         + QString::fromUtf8(body) + "\n" ;
}

int SMS::sendEsendex(QString number, QString from, QString message)
{
    QString URL = "https://api.esendex.co.uk/v2/messages" ;
    QString accountref = username.trimmed() ;

    // The sender is not sent: Esendex uses the account's sender
    Q_UNUSED(from) ;

    if (accountref.isEmpty()) {
        smsErrorData = "The Esendex account reference is blank. Set it as the username in the [sms-reminder] section of ContactManager.ini (from My Account in the Esendex portal)." ;
        return -1 ;
    }
    if (password.isEmpty()) {
        smsErrorData = "The Esendex API key is blank. Set it in Settings (the SMS password), from Developer Tools in the Esendex portal." ;
        return -1 ;
    }

    // Esendex wants the number in international form with a leading +
    number = number.remove(' ') ;
    if (number.isEmpty()) {
        smsErrorData = "There is no mobile number to send to." ;
        return -1 ;
    }
    if (!number.startsWith("+")) number = "+" + number ;

    QJsonObject recipient ;
    recipient.insert("msisdn", number) ;

    QJsonArray recipients ;
    recipients.append(recipient) ;

    QJsonObject text ;
    text.insert("text", message) ;

    QJsonObject root ;
    root.insert("accountReference", accountref) ;
    root.insert("channel", "SMS") ;
    root.insert("body", text) ;
    root.insert("recipients", recipients) ;

    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact) ;

    QUrl url(URL) ;
    QNetworkRequest request(url) ;
    QNetworkAccessManager manager ;
    QEventLoop eventLoop ;
    QNetworkReply *reply ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", password.toUtf8()) ;
    request.setTransferTimeout(SMS_TIMEOUT_MS) ;

    qDebug() << "SMS Connecting to " << URL ;

    reply = manager.post(request, json) ;
    eventLoop.exec() ;

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ;
    QString body = QString::fromUtf8(reply->readAll()).trimmed() ;
    int code ;

    if (reply->error()!=QNetworkReply::NoError && status==0) {
        // No reply: a timeout may still have been accepted by the service, so it is not sent again
        if (reply->error()==QNetworkReply::OperationCanceledError || reply->error()==QNetworkReply::RemoteHostClosedError) {
            smsErrorData = "No reply from Esendex in time, so the message may have been sent." ;
            code = -4 ;
        } else {
            smsErrorData = "Could not reach Esendex: " + reply->errorString() ;
            code = -1 ;
        }

    } else if (status==200 || status==201) {
        smsErrorData = body ;
        code = 1 ;

    } else if (status==400) {
        smsErrorData = "Esendex rejected the message (HTTP 400): " + body ;
        code = -3 ;

    } else if (status==401 || status==403) {
        smsErrorData = "Esendex rejected the API key or account reference (HTTP " + QString::number(status) + "). Check the username and the Settings password. Esendex said: " + body ;
        code = -3 ;

    } else if (status>=500) {
        // The service failed after receiving the request, so the message may have been sent
        smsErrorData = QString("Esendex failed (HTTP %1): %2").arg(status).arg(body) ;
        code = -4 ;

    } else {
        smsErrorData = QString("Esendex replied HTTP %1: %2").arg(status).arg(body) ;
        code = -3 ;
    }

    logSMSTransaction("send", loggedEsendexRequest("POST", URL, accountref, password, json), status, body, reply->errorString(), code) ;
    return code ;
}

int SMS::getEsendexBalance()
{
    QString URL = "https://api.esendex.co.uk/v2/balance" ;
    QString accountref = username.trimmed() ;

    if (accountref.isEmpty() || password.isEmpty()) {
        smsErrorData = "The Esendex account reference or API key is blank, so the balance cannot be read." ;
        return -2 ;
    }

    QUrl url(URL) ;
    QNetworkRequest request(url) ;
    QNetworkAccessManager manager ;
    QEventLoop eventLoop ;
    QNetworkReply *reply ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", password.toUtf8()) ;
    request.setRawHeader("accountReference", accountref.toUtf8()) ;
    request.setTransferTimeout(SMS_TIMEOUT_MS) ;

    reply = manager.get(request) ;
    eventLoop.exec() ;

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ;
    QString body = QString::fromUtf8(reply->readAll()).trimmed() ;
    int code ;

    if (reply->error()!=QNetworkReply::NoError && status==0) {
        smsErrorData = "Could not reach Esendex for the balance: " + reply->errorString() ;
        code = -2 ;

    } else if (status==200 || status==201) {
        // The balance is in pounds, as a decimal string: {"available": "2951.73346", "reserved": "0"}
        QJsonObject reply_json = QJsonDocument::fromJson(body.toUtf8()).object() ;
        bool ok = false ;
        double pounds = reply_json.value("available").toString().toDouble(&ok) ;
        if (ok) {
            code = qRound(pounds * 100) ;
        } else {
            smsErrorData = "Could not read the balance from the reply: " + body ;
            code = -3 ;
        }

    } else if (status==401 || status==403) {
        smsErrorData = "Esendex rejected the API key or account reference (HTTP " + QString::number(status) + ") for the balance. Esendex said: " + body ;
        code = -2 ;

    } else {
        smsErrorData = QString("Esendex replied HTTP %1 for the balance: %2").arg(status).arg(body) ;
        code = -4 ;
    }

    logSMSTransaction("balance", loggedEsendexRequest("GET", URL, accountref, password, QByteArray()), status, body, reply->errorString(), code) ;
    return code ;
}
