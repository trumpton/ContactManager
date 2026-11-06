//
// Test substitute for email.cpp, for the announcements bench (autotest/announcements).
//
// Linked in place of email.o in the bench's driver build only.  The program's Email class keeps its interface; nothing is
// sent to a server.  Each send is written as a file in CM_TEST_OUTBOX, with the result (sent or refused), the sender, the
// address, the subject, and the body.  The file's "sent:" time is the program's own clock (the fake clock, see clock/).
//
// CM_TEST_FAIL_EMAIL="address=N;address=M" refuses the first N sends to each address (999 refuses every send).  A refused
// send returns -1 with the reason in the error text, as a transport refusal does.
//
// Compiled against the program's headers (email.h).  Build: driver/build_driver.sh.
//

#include "email.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QTextStream>

// The address -> the sends still to be refused, read once per process
static bool refuseThisSend(const QString &to)
{
    static QMap<QString, int> plan ;
    static bool read = false ;
    if (!read) {
        read = true ;
        for (const QString &item : QString(qEnvironmentVariable("CM_TEST_FAIL_EMAIL")).split(';', Qt::SkipEmptyParts)) {
            QStringList parts = item.split('=') ;
            if (parts.size() == 2) plan.insert(parts.at(0).trimmed().toLower(), parts.at(1).trimmed().toInt()) ;
        }
    }
    auto it = plan.find(to.trimmed().toLower()) ;
    if (it == plan.end() || it.value() <= 0) return false ;
    it.value()-- ;
    return true ;
}

// Writes one outcome file to CM_TEST_OUTBOX.  The name starts with the time, so the files sort in the order sent.
static void writeOutcome(const QString &result, const QString &from, const QString &to, const QString &subject,
                         const QString &body, const QString &reason)
{
    static int counter = 0 ;
    QString dir = qEnvironmentVariable("CM_TEST_OUTBOX") ;
    QDir().mkpath(dir) ;
    QString name = QString("%1-%2-%3-email-%4.txt").arg(QDateTime::currentMSecsSinceEpoch())
                                                    .arg(QCoreApplication::applicationPid())
                                                    .arg(counter++)
                                                    .arg(result) ;
    QFile file(dir + "/" + name) ;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return ;
    QTextStream o(&file) ;
    o << "sent: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n"
      << "result: " << result << "\n"
      << "channel: email\n"
      << "from: " << from << "\n"
      << "to: " << to << "\n"
      << "subject: " << subject << "\n"
      << "reason: " << reason << "\n"
      << "body:\n" << body << "\n" ;
}

Email::Email(QString server, QString username, QString password, int port)
{
    this->server = server ;
    this->username = username ;
    this->password = password ;
    this->port = port ;
}

QString &Email::getErrorMessage()
{
    return errormessage ;
}

int Email::send(QString from, QString to, QString subject, QString body)
{
    if (qEnvironmentVariableIsEmpty("CM_TEST_OUTBOX")) {
        errormessage = "The bench's email stub is in use, but CM_TEST_OUTBOX is not set" ;
        return -3 ;
    }
    if (refuseThisSend(to)) {
        errormessage = "Refused by the bench's email stub" ;
        writeOutcome("refused", from, to, subject, body, errormessage) ;
        return -1 ;
    }
    errormessage.clear() ;
    writeOutcome("sent", from, to, subject, body, QString()) ;
    return 1 ;
}
