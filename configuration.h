#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QDialog>
#include <QSettings>
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
    QString settingDebugEnabled ;
    QString settingEncryptedEnabled ;
    QString settingEmailCommand ;
    QString settingContactSyncEnabled ;
    QString settingCalendarSyncEnabled ;
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
    bool encryptedEnabled() ;
    bool contactSyncEnabled() ;
    bool calendarSyncEnabled() ;

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
