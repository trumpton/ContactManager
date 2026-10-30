#include "email.h"
#include "smtp.h"

#include <QEventLoop>
#include <QTimer>

// The longest a mail conversation may take, from the connection to the server's last reply.  Without this, a server
// that stops replying would leave the app waiting for ever.
static const int EMAIL_TIMEOUT_MS = 60000 ;

// The usual port for secure (TLS) mail submission, used when the ini file gives none
static const int EMAIL_DEFAULT_PORT = 465 ;

Email::Email(QString server, QString username, QString password, int port)
{
    this->server = server ;
    this->username = username ;
    this->password = password ;
    this->port = (port>0) ? port : EMAIL_DEFAULT_PORT ;
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
    bool timedout = false ;

    QTimer watchdog ;
    watchdog.setSingleShot(true) ;
    QObject::connect(&watchdog, &QTimer::timeout, [&]() { timedout=true ; eventLoop.quit() ; }) ;
    QObject::connect(&smtp, SIGNAL(status(QString)), &eventLoop, SLOT(quit()));

    smtp.sendMail(from, to, subject, body) ;
    if (!smtp.isConnected()) {
        errormessage = "Could not connect to " + server ;
        return -2 ;
    }

    watchdog.start(EMAIL_TIMEOUT_MS) ;
    eventLoop.exec() ;
    watchdog.stop() ;

    if (timedout) {
        // The server did not finish, so the message may have been accepted: it is not sent again
        errormessage = "No reply from " + server + " in time" ;
        return -4 ;
    }

    errormessage = smtp.errorMessage() ;
    return smtp.success() ;
}
