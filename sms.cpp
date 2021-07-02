#include "sms.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QRegExp>
#include <QDebug>

SMS::SMS(QString& agent, QString& username, QString& password)
{
    this->password = password ;
    this->username = username ;
    this->agent = DEFAULT ;
    if (agent.toLower().compare("clockwork")==0) this->agent=Clockwork ;
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
        break ;
    }
    return false ;
}

int SMS::getBalance()
{
    switch (agent) {
    case Clockwork:
        return getClockworkBalance() ;
        break ;
    }
    return -1 ;

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

    params.append("key=").append(password).append("&to=").append(number).append("&content=").append(message) ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    qDebug() << "SMS Connecting to " << URL ;

    QNetworkAccessManager::NetworkAccessibility na = manager.networkAccessible() ;
    if (na != QNetworkAccessManager::Accessible) {
        // Connection Failure (network not up)
        qDebug() << "Connection Failed" ;
        return -2 ;

    } else {

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

    params.append("key=").append(password) ;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    if (manager.networkAccessible() == QNetworkAccessManager::NotAccessible) {

        // Connection Error
        return -2 ;

    } else {

        reply = manager.post(request, params) ;
        eventLoop.exec() ;

        // Error response returned
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)!=200) return -4 ;

        QString balance = reply->readAll().toLower().trimmed() ;
        QRegExp exp("[^0-9]*([0-9]+)[\\.:]([0-9]+)[^0-9]*") ;
        if (exp.indexIn(balance)>=0) {
            return exp.cap(1).toInt() * 100 + exp.cap(2).toInt() ;
        }

        // Unable to decode
        return -3 ;

    }
}

