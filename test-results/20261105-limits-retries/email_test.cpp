#include <QFile>
#include <QMap>
#include <QTextStream>
#include <QDateTime>
#include <QProcessEnvironment>
#include "email.h"
#include <QCoreApplication>
#include "smtp.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"

#include <QDir>
#include <QEventLoop>
#include <QTimer>

// The longest a mail conversation may take, from the connection to the server's last reply.  Without this, a server
// that stops replying would leave the app waiting for ever.
static const int EMAIL_TIMEOUT_MS = 60000 ;

// The usual port for secure (TLS) mail submission, used when the ini file gives none
static const int EMAIL_DEFAULT_PORT = 465 ;

// Debug: each mail conversation is written to a file in the database folder, numbered like the Google debug files
// (NNNN-email.emaillog), when ini [email-reminder] debug=yes.  The transcript holds the lines sent and received, with
// the login masked.  The message text is written in full.
static void logEmailTransaction(const QString &transcript, int code, const QString &error)
{
    if (!gConf || !gConf->debugEmailEnabled()) return ;

    // The next number follows the highest one already there
    QDir logdir(gConf->getDatabasePath()) ;
    int next = 1 ;
    for (const QString &file : logdir.entryList(QStringList() << "????-*.emaillog", QDir::Files)) {
        next = qMax(next, file.left(4).toInt()+1) ;
    }

    QString text = "TRANSCRIPT>>\n" + transcript + "\n"
                 + "Error: " + error + "\n"
                 + "Result: " + QString::number(code) + "\n" ;

    QString filename = QString("%1").arg(next, 4, 10, QChar('0')) + "-email.emaillog" ;
    writeToFile(gConf->getDatabasePath() + "/" + filename, text) ;
}

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

// TEST BUILD: CM_TEST_FAIL_EMAIL="address=N;address=M" makes the first N (or M) sends to each address fail, so the retries can be
// tested (issue #110).  Counted per process; the sends that fail are not written.
static bool failEmailSend(const QString &to)
{
    static QMap<QString, int> plan ;
    static bool read = false ;
    if (!read) {
        read = true ;
        for (const QString &item : QString(qEnvironmentVariable("CM_TEST_FAIL_EMAIL")).split(';', Qt::SkipEmptyParts)) {
            QStringList parts = item.split('=') ;
            if (parts.size()==2) plan.insert(parts.at(0).trimmed(), parts.at(1).toInt()) ;
        }
    }
    auto it = plan.find(to.trimmed()) ;
    if (it==plan.end() || it.value()<=0) return false ;
    it.value()-- ;
    return true ;
}

int Email::send(QString from, QString to, QString subject, QString body)
{

    // TEST BUILD: the message is written to CM_TEST_OUTBOX instead of being sent
    if (qEnvironmentVariableIsSet("CM_TEST_OUTBOX")) {
        if (failEmailSend(to)) return -1 ;
        QString file = QString(qEnvironmentVariable("CM_TEST_OUTBOX")) + "/" + QString::number(QDateTime::currentMSecsSinceEpoch()) + "-email.txt" ;
        QFile out(file) ;
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) return -3 ;
        QTextStream o(&out) ;
        o << "sent: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n" ;
        o << "from: " << from << "\n" << "to: " << to << "\n" << "subject: " << subject << "\n" << "body:\n" << body << "\n" ;
        out.close() ;
        return 1 ;
    }

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
        logEmailTransaction(smtp.getTranscript(), -2, errormessage) ;
        return -2 ;
    }

    watchdog.start(EMAIL_TIMEOUT_MS) ;
    eventLoop.exec() ;
    watchdog.stop() ;

    if (timedout) {
        // The server did not finish, so the message may have been accepted: it is not sent again
        errormessage = "No reply from " + server + " in time" ;
        logEmailTransaction(smtp.getTranscript(), -4, errormessage) ;
        return -4 ;
    }

    errormessage = smtp.errorMessage() ;
    int code = smtp.success() ;
    logEmailTransaction(smtp.getTranscript(), code, errormessage) ;
    return code ;
}
