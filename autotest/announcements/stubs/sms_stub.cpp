//
// Test substitute for sms.cpp, for the announcements bench (autotest/announcements).
//
// Linked in place of sms.o in the bench's driver build only.  The SMS class keeps its interface; nothing is sent to a
// service.  Each send is written as a file in CM_TEST_OUTBOX: the number, and the text (the first line of the template,
// which is the text sent), with the result.  The file's "sent:" time is the program's own clock (the fake clock).
//
// CM_TEST_FAIL_SMS="number=N:reason" refuses the first N sends to the number, with the reason (the number is the form the
// program sends: digits, with the country code, for example 447700900003).  A refused send returns -3 with the reason as the
// error text, as a transport refusal does.
//
// internationalNumber() is the program's own rule (sms.cpp), copied so that the numbers the stub sees are the ones sent.
//

#include "sms.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QTextStream>

// The number -> (the sends still to be refused, and the reason), read once per process
struct SmsFailure {
    int remaining ;
    QString reason ;
} ;

static SmsFailure *failureFor(const QString &number)
{
    static QMap<QString, SmsFailure> plan ;
    static bool read = false ;
    if (!read) {
        read = true ;
        for (const QString &item : QString(qEnvironmentVariable("CM_TEST_FAIL_SMS")).split(';', Qt::SkipEmptyParts)) {
            int eq = item.indexOf('=') ;
            int colon = item.indexOf(':') ;
            if (eq < 0 || colon < eq) continue ;
            SmsFailure f ;
            f.remaining = item.mid(eq + 1, colon - eq - 1).trimmed().toInt() ;
            f.reason = item.mid(colon + 1).trimmed() ;
            plan.insert(item.left(eq).trimmed(), f) ;
        }
    }
    auto it = plan.find(number) ;
    if (it == plan.end() || it.value().remaining <= 0) return nullptr ;
    it.value().remaining-- ;
    return &it.value() ;
}

static void writeOutcome(const QString &result, const QString &from, const QString &number, const QString &text,
                         const QString &reason)
{
    static int counter = 0 ;
    QString dir = qEnvironmentVariable("CM_TEST_OUTBOX") ;
    QDir().mkpath(dir) ;
    QString name = QString("%1-%2-%3-sms-%4.txt").arg(QDateTime::currentMSecsSinceEpoch())
                                                  .arg(QCoreApplication::applicationPid())
                                                  .arg(counter++)
                                                  .arg(result) ;
    QFile file(dir + "/" + name) ;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return ;
    QTextStream o(&file) ;
    o << "sent: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n"
      << "result: " << result << "\n"
      << "channel: sms\n"
      << "from: " << from << "\n"
      << "to: " << number << "\n"
      << "subject: " << text << "\n"
      << "reason: " << reason << "\n"
      << "body:\n" ;
}

SMS::SMS(QString &agent, QString &username, QString &password)
{
    (void)agent ;
    this->password = password ;
    this->username = username ;
    this->agent = Esendex ;
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
    return true ;
}

QString &SMS::getErrorMessage()
{
    return smsErrorData ;
}

int SMS::getBalance()
{
    return 0 ;
}

int SMS::send(QString number, QString from, QString message)
{
    if (qEnvironmentVariableIsEmpty("CM_TEST_OUTBOX")) {
        smsErrorData = "The bench's SMS stub is in use, but CM_TEST_OUTBOX is not set" ;
        return -3 ;
    }
    SmsFailure *failure = failureFor(number) ;
    if (failure) {
        smsErrorData = failure->reason ;
        writeOutcome("refused", from, number, message, failure->reason) ;
        return -3 ;
    }
    smsErrorData.clear() ;
    writeOutcome("sent", from, number, message, QString()) ;
    return 1 ;
}
