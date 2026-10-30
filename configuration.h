#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QDialog>
#include <QSettings>
#include <QTime>
#include "contactdatabase.h"
#include "googleaccess.h"
#include "../Lib/iniconfig.h"
#include "../Lib/encryption.h"

namespace Ui {
class Configuration;
}

enum DatabaseMaster {
    ContactManagerMaster = 0,
    GoogleMaster = 1
};

#define MSG_EMAIL_CONFIRMATION 1
#define MSG_EMAIL_DELETION 2
#define MSG_EMAIL_UPDATE 3
#define MSG_EMAIL_REMINDER 4
#define MSG_SMS_CONFIRMATION 5
#define MSG_SMS_DELETION 6
#define MSG_SMS_UPDATE 7
#define MSG_SMS_REMINDER 8

// Max Number of SMS/Emails to send for a single appointment
// (used to trap send bugs to prevent client being bombarded)
#define MAXMESSAGERETRIES 10

class Configuration : public QDialog
{
    Q_OBJECT

private:
    QString settingCodec ;
    QString settingIdMe ;
    QString settingDatabaseDir ;
    QString settingBackupDir ;
    QString settingJournalDir ;
    QString settingEasyNotepadExe ;
    QString settingBackupDate ;
    QString settingGoogleUsername ;
    QString settingGoogleRefreshToken ;
    QString settingLocalDiallingCode ;
    QString settingCountryDiallingCode ;
    QString settingWorkingDirectory ;
    QString settingLastGoogleContactSyncDate ;
    QString settingLastGoogleCalendarSyncDate ;
    QString settingLastGoogleContactSyncToken ;
    QString settingLastGoogleCalendarSyncToken ;
    QString settingSMSAgent ;
    QString settingSMSUsername ;
    QString settingSMSPassword ;
    QString settingSMTPServer ;
    QString settingSMTPUsername ;
    QString settingSMTPPort ;
    QString settingSMTPPassword ;
    QString settingEmailCommand ;
    DatabaseMaster settingDatabaseMaster ;
    Encryption *enc ;

public:
    explicit Configuration(QWidget *parent = 0);
    ~Configuration();

    bool save(QString filename) ;

    bool SetupForm(ContactDatabase *db, GoogleAccess *ga) ;
    bool SaveForm() ;

    // INI File Access Functions
    QString &getDatabasePath() ;
    QString &getBackupPath() ;
    QString &getJournalPath(QString userName) ;
    QString &getEasyNotepadPath() ;
    QString getEmailCommand(QString name, QString to) ;
    QString &getLocalDiallingCode() ;
    QString &getCountryDiallingCode() ;
    QString &getCodec() ;
    QString &getSMSAgent() ;
    QString &getSMSUsername() ;
    QString &getSMTPServer() ;
    QString &getSMTPUsername() ;
    QString &getSMTPPort() ;
    QString getMessageFile(int index) ;
    bool contactManagerMaster() ;
    bool googleMaster() ;
    bool debugGoogleEnabled() ;

    // The sender for SMS, if set in ini [sms-reminder] sender (a registered name), overriding the template's sender line
    QString getSMSSender() ;

    // The date the SMS API key expires, kept in the settings store with the API key, written day/month/year
    // (25/01/2035).  Invalid if not set.
    QDate getSMSKeyExpiry() ;

    // The From address of reminder emails, if set in ini [email-reminder] sender (as "Name <address>"), overriding the
    // template's first line
    QString getEmailSender() ;

    // Whether each transaction with the SMS service is written to a debug file (ini [sms-reminder] debug=yes or no)
    bool debugSMSEnabled() ;

    // Whether each email conversation is written to a debug file (ini [email-reminder] debug=yes or no)
    bool debugEmailEnabled() ;
    bool encryptedEnabled() ;
    bool contactSyncEnabled() ;
    bool calendarSyncEnabled() ;
    bool calendarShowBirthdays() ;
    int calendarWindowBefore() ;            // Days before today that appointments are shown and kept from (default 90)
    int calendarWindowAfter() ;             // Days after today that appointments are shown and kept to (default 730)

    // QSettings Access Functions
    QString &getMe() ;
    QString &getGoogleOauth2RefreshToken() ;
    QString &getGoogleUsername() ;
    QString &getLastGoogleContactSyncDate() ;
    QString &getLastGoogleCalendarSyncDate() ;
    QString &getLastGoogleContactSyncToken() ;
    QString &getLastGoogleCalendarSyncToken() ;
    QString& getSMTPPassword() ;
    QString& getSMSPassword() ;
    DatabaseMaster getDatabaseMaster() ;
    bool getEnableReminders() ;

    // Reminders are for appointments that start between these many hours from now (ini [reminders])
    int getReminderWindowFromHours() ;
    int getReminderWindowToHours() ;

    // Reminders are sent from the start time, up to but not including the end time, in local time (ini [reminders])
    QTime getSendWindowStart() ;
    QTime getSendWindowEnd() ;

    // A new appointment waits this many minutes before its messages are sent (ini [reminders], default 5)
    int getWaitBeforeSendMinutes() ;

    // A changed appointment's update waits this many minutes after its last change, so that several quick edits send
    // one update (waitbeforeupdatemins in [reminders], default 5)
    int getWaitBeforeUpdateMinutes() ;

    // The most messages sent to one contact in a day (maxmessagesperdaypercontact in [reminders], default 10)
    int getMaxMessagesPerContactPerDay() ;

    // The messages sent to a contact on a day (kept in the settings store), and adding one
    int getContactMessagesToday(const QString &contactid, const QString &day) ;
    void addContactMessageToday(const QString &contactid, const QString &day) ;

    // Whether an .ics appointment is attached to emails (ini [reminders], attachics=yes or no, default no)
    bool getAttachICS() ;

    void setLastGoogleContactSyncDate(QString syncDate) ;
    void setLastGoogleCalendarSyncDate(QString syncDate) ;
    void setLastGoogleContactSyncToken(QString syncToken) ;
    void setLastGoogleCalendarSyncToken(QString syncToken) ;

    QString &getBackupDate() ;
    void setBackupDate(QString dt) ;
    int getBackupSequence() ;
    void setBackupSequence(int seq) ;

    QString getDebug(QString key) ;

    // Encryption Info
    void setEncryption(Encryption *enc) ;
    Encryption *encryption() ;

//    void setWorkingDirectory(QString dir) ;
//    void setCodec(QString val) ;
//    void setBackupPath() ;


private slots:
    void on_Authenticate_clicked();
    void on_ResetLastSync_clicked();

    void on_conflictGoogle_clicked();

    void on_conflictContactManager_clicked();

private:
    QString expandVars(QString str, QString name=QString(""), QString to=QString("")) ;
    QString tidyPath(QString path, bool appendslash=true) ;

    IniConfig ini ;
    QSettings *settings ;
    ContactDatabase emptydatabase ;
    GoogleAccess emptygoogleaccess ;
    QString googleRefreshToken ;
    GoogleAccess *googleaccess ;
    ContactDatabase *db ;
    Ui::Configuration *ui;

};

extern QString gSavePath ;
extern QString gContactSavePath ;
extern QString gCalendarSavePath ;

extern Configuration *gConf ;

#endif // CONFIGURATION_H
