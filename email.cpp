#include "email.h"
#include "smtp.h"

#include <QEventLoop>

Email::Email(QString server, QString username, QString password, int port)
{
    this->server = server ;
    this->username = username ;
    this->password = password ;
    this->port = (port<25) ? 25 : port ;
}

QString& Email::getErrorMessage()
{
    return errormessage ;
}

int Email::send(QString from, QString to, QString subject, QString body)
{
    if (username.isEmpty() || password.isEmpty() || server.isEmpty()) {
        errormessage = "Email is not configured" ;
        return 0 ;
    }

    Smtp smtp(username, password, server, port) ;
    QEventLoop eventLoop ;

    QObject::connect(&smtp, SIGNAL(status(QString)), &eventLoop, SLOT(quit()));
    smtp.sendMail(from, to, subject, body) ;
    if (smtp.isConnected()) {
        eventLoop.exec() ;
        errormessage = smtp.errorMessage() ;
        return smtp.success() ;
    }

    errormessage = "Could not connect to " + server ;
    return 0 ;
}
