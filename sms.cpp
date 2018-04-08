#include "sms.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QRegExp>

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

bool SMS::send(QString number, QString from, QString message)
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

bool SMS::sendClockwork(QString number, QString from, QString message)
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

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    if (manager.networkAccessible() == QNetworkAccessManager::NotAccessible) {

        return false ;

    } else {

        reply = manager.post(request, params) ;
        eventLoop.exec() ;

        smsErrorData = reply->readAll().trimmed() ;
        bool iserror = smsErrorData.left(5).toLower().compare("error")==0 ;

        return !iserror && (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute))==200 ;

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

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    if (manager.networkAccessible() == QNetworkAccessManager::NotAccessible) {

        return -1 ;

    } else {

        reply = manager.post(request, params) ;
        eventLoop.exec() ;

        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)!=200) return -1 ;

        QString balance = reply->readAll().toLower().trimmed() ;
        QRegExp exp("[^0-9]*([0-9]+)[\\.:]([0-9]+)[^0-9]*") ;
        if (exp.indexIn(balance)>=0) {
            return exp.cap(1).toInt() * 100 + exp.cap(2).toInt() ;
        }

        return -2 ;

    }
}

