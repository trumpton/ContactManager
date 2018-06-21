#include "configuration.h"
#include "ui_configuration.h"

#include "contactdatabase.h"
#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "../Lib/iniconfig.h"

#include <QStandardPaths>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QTextCodec>

QString gSavePath ;
QString gContactSavePath ;
QString gCalendarSavePath ;
Configuration *gConf ;

Configuration::Configuration(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Configuration)
{
    settingCodec = "" ;
    settingIdMe = "" ;
    settingDatabaseDir = "" ;
    settingBackupDir = "" ;
    settingJournalDir = "" ;
    settingEasyNotepadExe = "" ;
    settingGoogleUsername = "" ;
    settingGoogleRefreshToken = "" ;
    settingLocalDiallingCode = "" ;
    settingCountryDiallingCode = "" ;
    settingWorkingDirectory = "" ;
    settingLastGoogleContactSyncDate = "" ;
    settingLastGoogleCalendarSyncDate = "" ;
    settingSMSAgent = "" ;
    settingSMSUsername = "" ;
    settingSMSPassword = "" ;
    settingSMTPServer = "" ;
    settingSMTPUsername = "" ;
    settingSMTPPort = "" ;
    settingSMTPPassword = "" ;
    settingDebugEnabled = "" ;
    settingDatabaseMaster = ContactManagerMaster ;

    ui->setupUi(this);
    settings = new QSettings("trumpton.org.uk", "ContactManager");

    // Load INI file from Data directory, TrumptonApps directory, otherwise application directory

    bool iniok=false ;
    QString inifile ;

    if (!iniok) {
        inifile = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QString("/ContactManager/ContactManager.ini") ;
        iniok = ini.init(inifile) ;
    }

    if (!iniok) {
        inifile = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QString("/TrumptonApps/ContactManager.ini") ;
        iniok = ini.init(inifile) ;
    }

    if (!iniok) {
        inifile = QApplication::applicationDirPath() + QString("/ContactManager.ini") ;
        iniok = ini.init(inifile) ;
    }

    if (!iniok) {
        errorOkDialog(this, "Contact Manager Error", "Invalid configuration or Unable to find ContactManager.ini.") ;
    }

    QTextCodec::setCodecForLocale(QTextCodec::codecForName(getCodec().toLatin1().constData()));

}

Configuration::~Configuration()
{
    delete settings ;
    delete ui;
}

bool Configuration::SetupForm(ContactDatabase *currentdatabase, GoogleAccess *ga)
{

    if (ga==NULL || currentdatabase==NULL) {
        // Safety Check
        return false ;
    }

    db = currentdatabase ;
    googleaccess = ga ;

    // Refresh the list
    ui->IdentifyMe->clear() ;
    ui->IdentifyMe->addItem("-- Select Contact --", "") ;
    for (int i=0; i<db->size(); i++) {
        Contact&c = db->getContact(i) ;
        if (!c.isSet(Contact::Deleted) && !c.isSet(Contact::Hidden)) {
            QString name=c.getFormattedName() ;
            QString id=c.getField(Contact::ID) ;
            ui->IdentifyMe->addItem(name, id);
        }
    }
    ui->IdentifyMe->setCurrentIndex(ui->IdentifyMe->findData(getMe()));

    // Load the Associated Google Username
    ui->googleUsername->setText(getGoogleUsername()) ;

    // Set the last sync date
    ui->lastContactSync->setText(getLastGoogleContactSyncDate()) ;
    ui->lastCalendarSync->setText(getLastGoogleCalendarSyncDate()) ;

    // Load the SMTP Settings
    ui->emailAccount->setText(getSMTPServer() + QString(": ") + getSMTPUsername()) ;
    ui->emailPassword->setText(getSMTPPassword()) ;

    // Load the SMS Settings
    QString smsAccountName = getSMSAgent() ;
    if (!getSMSUsername().isEmpty()) {
        smsAccountName = smsAccountName + QString(": ") + getSMSUsername() ;
    }
    ui->smsAccount->setText(smsAccountName) ;
    ui->smsPassword->setText(getSMSPassword()) ;

    ui->enableReminders->setChecked(getEnableReminders()) ;

    switch (getDatabaseMaster()) {
    default:
    case ContactManagerMaster:
        ui->conflictContactManager->setChecked(true) ;
        break ;
    case GoogleMaster:
        ui->conflictGoogle->setChecked(true) ;
        break ;
    case LatestMaster:
        ui->conflictLatest->setChecked(true) ;
        break ;
    }

    // Show ini file path
    ui->iniFilePath->setText(ini.canonicalPath()) ;

    // Load the refresh token
    googleRefreshToken = settings->value("googlerefreshtoken").toString() ;

    return true ;
}

bool Configuration::SaveForm()
{
    if (db==NULL) {
        // Safety Check
        return false ;
    }

    settings->setValue("idme", ui->IdentifyMe->currentData()) ;
    settings->setValue("googlerefreshtoken", googleRefreshToken) ;
    settings->setValue("googleusername", ui->googleUsername->text()) ;
    settings->setValue("smtppassword", ui->emailPassword->text());
    settings->setValue("smspassword", ui->smsPassword->text()) ;
    if (ui->conflictContactManager->isChecked())
        settings->setValue("databasemaster", ContactManagerMaster) ;
    else if (ui->conflictGoogle->isChecked())
        settings->setValue("databasemaster", GoogleMaster) ;
    else
        settings->setValue("databasemaster", LatestMaster) ;
    if (ui->enableReminders->isChecked()) {
        settings->setValue("enablereminders", "yes") ;
    } else {
        settings->setValue("enablereminders", "no") ;
    }
    return true ;
}

bool Configuration::contactManagerMaster()
{
    return getDatabaseMaster() == ContactManagerMaster ;
}

bool Configuration::googleMaster()
{
    return getDatabaseMaster() == GoogleMaster ;
}

bool Configuration::latestMaster()
{
    return getDatabaseMaster() == LatestMaster ;
}

bool Configuration::debugGoogleEnabled()
{
    QString m = ini.get("google", "debug") ;
    return (m.compare("true", Qt::CaseInsensitive)==0 || m.compare("yes", Qt::CaseInsensitive)==0) ;
}

QString& Configuration::getCodec()
{
    settingCodec = ini.get("general", "codec") ;
    if (settingCodec.isEmpty()) settingCodec="UTF-8" ;
    return settingCodec ;
}

QString Configuration::getEmailCommand(QString name, QString to)
{
    QString cmd = ini.get("email", "command") ;
    cmd = expandVars(cmd, name, to) ;
    return cmd ;
}

DatabaseMaster Configuration::getDatabaseMaster()
{
    settingDatabaseMaster = (DatabaseMaster)settings->value("databasemaster").toInt() ;
    return settingDatabaseMaster ;
}

QString& Configuration::getSMSAgent()
{
    settingSMSAgent = ini.get("sms-reminder","agent") ;
    return settingSMSAgent ;
}

QString& Configuration::getSMSUsername()
{
    settingSMSUsername = ini.get("sms-reminder","username") ;
    return settingSMSUsername ;
}

QString& Configuration::getSMSPassword()
{
    settingSMSPassword = settings->value("smspassword").toString() ;
    return settingSMSPassword ;
}

QString& Configuration::getMe()
{
    settingIdMe = settings->value("idme").toString() ;
    return settingIdMe ;
}

QString& Configuration::getGoogleOauth2RefreshToken()
{
    settingGoogleRefreshToken = settings->value("googlerefreshtoken").toString() ;
    return settingGoogleRefreshToken ;
}


QString& Configuration::getLastGoogleContactSyncDate()
{
    settingLastGoogleContactSyncDate = settings->value("lastgooglecontactsyncdate").toString() ;
    if (settingLastGoogleContactSyncDate.isEmpty()) setLastGoogleContactSyncDate(QString("")) ;
    return settingLastGoogleContactSyncDate ;
}

QString& Configuration::getLastGoogleCalendarSyncDate()
{
    settingLastGoogleCalendarSyncDate = settings->value("lastgooglecalendarsyncdate").toString() ;
    if (settingLastGoogleCalendarSyncDate.isEmpty()) setLastGoogleCalendarSyncDate(QString("")) ;
    return settingLastGoogleCalendarSyncDate ;
}

void Configuration::setLastGoogleCalendarSyncDate(QString syncDate)
{
    if (syncDate.isEmpty()) syncDate = QString("1970-01-01T00:00:00Z") ;
    settingLastGoogleCalendarSyncDate = syncDate ;
    settings->setValue("lastgooglecalendarsyncdate", settingLastGoogleCalendarSyncDate) ;
}

void Configuration::setLastGoogleContactSyncDate(QString syncDate)
{
    if (syncDate.isEmpty()) syncDate = QString("1970-01-01T00:00:00Z") ;
    settingLastGoogleContactSyncDate = syncDate ;
    settings->setValue("lastgooglecontactsyncdate", settingLastGoogleContactSyncDate) ;
}

void Configuration::setLastGoogleContactSyncToken(QString syncToken)
{
    settingLastGoogleContactSyncToken = syncToken ;
    settings->setValue("lastgooglecontactsynctoken", settingLastGoogleContactSyncToken) ;
}

void Configuration::setLastGoogleCalendarSyncToken(QString syncToken)
{
    settingLastGoogleCalendarSyncToken = syncToken ;
    settings->setValue("lastgooglecalendarsynctoken", settingLastGoogleCalendarSyncToken) ;
}

QString& Configuration::getLastGoogleContactSyncToken()
{
    settingLastGoogleContactSyncToken = settings->value("lastgooglecontactsynctoken").toString() ;
    return settingLastGoogleContactSyncToken ;
}

QString& Configuration::getLastGoogleCalendarSyncToken()
{
    settingLastGoogleCalendarSyncToken = settings->value("lastgooglecalendarsynctoken").toString() ;
    return settingLastGoogleCalendarSyncToken ;
}

QString& Configuration::getDatabasePath()
{
    settingDatabaseDir = ini.get("database", "path") ;
    if (settingDatabaseDir.isEmpty()) settingDatabaseDir = QDir::homePath() + "/TrumptonApps/ContactManager" ;
    settingDatabaseDir = expandVars(settingDatabaseDir) ;
    settingDatabaseDir = tidyPath(settingDatabaseDir) ;
    return  settingDatabaseDir ;
}


QString& Configuration::getBackupPath()
{
    settingBackupDir = getDatabasePath() + "backup" + QDir::toNativeSeparators("/") ;
    return settingBackupDir ;
}


QString& Configuration::getJournalPath(QString userName)
{
    settingJournalDir = ini.get("journal", "filename") ;
    settingJournalDir = expandVars(settingJournalDir, userName.replace("/","").replace("\\","")) ;
    settingJournalDir = tidyPath(settingJournalDir, false) ;
    return settingJournalDir ;
}

QString& Configuration::getEasyNotepadPath()
{
    settingEasyNotepadExe = ini.get("journal", "easynotepadexe") ;
    settingEasyNotepadExe = expandVars(settingEasyNotepadExe) ;
    return settingEasyNotepadExe ;
}


QString& Configuration::getGoogleUsername()
{
    settingGoogleUsername = settings->value("googleusername").toString() ;
    return settingGoogleUsername ;
}

QString& Configuration::getLocalDiallingCode()
{
    settingLocalDiallingCode = ini.get("phone", "localcode") ;
    return settingLocalDiallingCode ;
}

QString& Configuration::getCountryDiallingCode()
{
    settingCountryDiallingCode = ini.get("phone", "countrycode") ;
    return settingCountryDiallingCode ;
}

QString& Configuration::getSMTPServer()
{
    settingSMTPServer = ini.get("email-reminder", "server") ;
    return settingSMTPServer ;
}

QString& Configuration::getSMTPUsername()
{
    settingSMTPUsername = ini.get("email-reminder", "username") ;
    return settingSMTPUsername ;
}

QString& Configuration::getSMTPPort()
{
    settingSMTPPort = ini.get("email-reminder", "port") ;
    return settingSMTPPort ;
}

QString& Configuration::getSMTPPassword()
{
    settingSMTPPassword = settings->value("smtppassword").toString() ;
    return settingSMTPPassword ;
}

bool Configuration::getEnableReminders()
{
    QString settingEnableReminders = settings->value("enablereminders").toString() ;
    return (settingEnableReminders.compare("yes")==0) ;
}

QString& Configuration::getBackupDate()
{
    settingBackupDate = settings->value("backupdate").toString() ;
    return settingBackupDate ;
}

void Configuration::setBackupDate(QString dt)
{
    settings->setValue("backupdate", dt) ;
}

int Configuration::getBackupSequence()
{
    int i = settings->value("backupsequence").toInt() ;
    return i ;
}

void Configuration::setBackupSequence(int seq)
{
    settings->setValue("backupsequence", QString::number(seq)) ;
}

QString Configuration::getDebug(QString key)
{
    static QString debugResult ;
    if (!settings->value(key).isValid()) {
        debugResult="" ;
    } else {
        debugResult=settings->value(key).toString() ;
    }
    return debugResult ;
}

void Configuration::on_Authenticate_clicked()
{
    if (googleaccess==NULL) {
        // Safety Check
        return ;
    }

    QString googleusername ;
    googleRefreshToken=googleaccess->Authorise() ;
    googleusername=googleaccess->getUsername() ;
    ui->googleUsername->setText(googleusername) ;

    // 'press' the sync reset button
    on_ResetLastSync_clicked();
}


QString Configuration::getMessageFile(int index)
{
    QString path ;

      switch (index) {
        case MSG_EMAIL_CONFIRMATION: path=ini.get("message", "emailconfirmation") ; break ;
        case MSG_EMAIL_DELETION: path=ini.get("message", "emaildelete") ; break ;
        case MSG_EMAIL_UPDATE: path=ini.get("message", "emailupdate") ; break ;
        case MSG_EMAIL_REMINDER: path=ini.get("message", "emailreminder") ; break ;
        case MSG_SMS_CONFIRMATION: path=ini.get("message", "smsconfirmation") ; break ;
        case MSG_SMS_DELETION: path=ini.get("message", "smsdelete") ; break ;
        case MSG_SMS_UPDATE: path=ini.get("message", "smsupdate") ; break ;
        case MSG_SMS_REMINDER: path=ini.get("message", "smsreminder") ; break ;
        default: break ;
        }

    return tidyPath(path, false) ;
}

QString Configuration::expandVars(QString str, QString name, QString to)
{
    str = str.replace("$NAME", name) ;
    str = str.replace("$TO", to) ;
    str = str.replace("$HOME", QDir::homePath()) ;
    str = str.replace("$CONFIG", QDir::homePath() + QDir::toNativeSeparators("/") + "TrumptonApps") ;
    str = str.replace("$INI", ini.canonicalPath()) ;
    str = str.replace("$APPDIR", QApplication::applicationDirPath()) ;
    return str ;
}

QString Configuration::tidyPath(QString path, bool appendslash)
{
    path = QDir::toNativeSeparators(path) ;
    if (appendslash && path.right(1).compare(QDir::toNativeSeparators("/"))!=0)
        path = path + QDir::toNativeSeparators("/") ;
    return path ;
}

void Configuration::on_ResetLastSync_clicked()
{
    setLastGoogleCalendarSyncDate(QString("")) ;
    setLastGoogleContactSyncDate(QString("")) ;
    setLastGoogleCalendarSyncToken(QString("")) ;
    setLastGoogleContactSyncToken(QString("")) ;
    ui->lastContactSync->setText(getLastGoogleContactSyncDate()) ;
    ui->lastCalendarSync->setText(getLastGoogleCalendarSyncDate()) ;
}
