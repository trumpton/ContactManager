#include "configuration.h"
#include "ui_configuration.h"

#include "contactdatabase.h"
#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "../Lib/iniconfig.h"

#include <QStandardPaths>
#include <QRegularExpression>

static QDate parseExpiryDate(QString text) ;
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
    ui->smsKeyExpiry->setText(settings->value("smsapikeyexpires").toString()) ;

    switch (getDatabaseMaster()) {
    default:
    case ContactManagerMaster:
        ui->conflictContactManager->setChecked(true) ;
        ui->enableReminders->setEnabled(true) ;
        ui->enableReminders->setChecked(getEnableReminders()) ;
        break ;
    case GoogleMaster:
        ui->conflictGoogle->setChecked(true) ;
        ui->enableReminders->setEnabled(false) ;
        ui->enableReminders->setChecked(false) ;
        break ;
    }

    // Load Sync Enabled Status
    ui->checkBox_syncCalendar->setChecked(calendarSyncEnabled()) ;
    ui->checkBox_syncContacts->setChecked(contactSyncEnabled()) ;

    // Load flags
    ui->showBirthdays->setChecked(calendarShowBirthdays()) ;
    ui->windowBefore->setValue(calendarWindowBefore()) ;
    ui->windowAfter->setValue(calendarWindowAfter()) ;

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

    // The API key expiry date, day/month/year, or blank.  The warning at startup only shows if it is set.
    // Stored in the standard form (25/01/2035), so 25/1/35 is kept as 25/01/2035
    QString keyexpiry = ui->smsKeyExpiry->text().trimmed() ;
    QDate expiry = parseExpiryDate(keyexpiry) ;
    if (!keyexpiry.isEmpty() && !expiry.isValid()) {
        warningOkDialog(this, "SMS API key expiry", "The expiry date must be day/month/year, for example 25/01/2035, or left blank.") ;
    }
    settings->setValue("smsapikeyexpires", expiry.isValid() ? expiry.toString("dd/MM/yyyy") : keyexpiry) ;

    if (ui->conflictContactManager->isChecked())
        settings->setValue("databasemaster", ContactManagerMaster) ;
    else if (ui->conflictGoogle->isChecked())
        settings->setValue("databasemaster", GoogleMaster) ;

    if (ui->enableReminders->isChecked()) {
        settings->setValue("enablereminders", "yes") ;
    } else {
        settings->setValue("enablereminders", "no") ;
    }

    if (ui->showBirthdays->isChecked()) {
        settings->setValue("showbirthdays", "yes") ;
    } else {
        settings->setValue("showbirthdays", "no") ;
    }

    settings->setValue("windowbefore", QString::number(ui->windowBefore->value())) ;
    settings->setValue("windowafter", QString::number(ui->windowAfter->value())) ;


    if (ui->checkBox_syncCalendar->isChecked())
        settings->setValue("calendarsync", "yes") ;
    else
        settings->setValue("calendarsync", "no") ;

    if (ui->checkBox_syncContacts->isChecked())
        settings->setValue("contactsync", "yes") ;
    else
        settings->setValue("contactsync", "no") ;

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

bool Configuration::debugGoogleEnabled()
{
    QString m = ini.get("google", "debug") ;
    return (m.compare("true", Qt::CaseInsensitive)==0 || m.compare("yes", Qt::CaseInsensitive)==0) ;
}

QString Configuration::getSMSSender()
{
    return ini.get("sms-reminder", "sender").trimmed() ;
}

// A date typed as day/month/year: the day and month may have one or two digits, and the year two or four (25/1/35,
// 25/01/2035).  Invalid if it is not a date.
static QDate parseExpiryDate(QString text)
{
    QStringList parts = text.trimmed().split(QRegularExpression("[/.-]")) ;
    if (parts.size()!=3) return QDate() ;

    bool okday, okmonth, okyear ;
    int day = parts.at(0).toInt(&okday) ;
    int month = parts.at(1).toInt(&okmonth) ;
    int year = parts.at(2).toInt(&okyear) ;
    if (!okday || !okmonth || !okyear) return QDate() ;

    if (parts.at(2).length()<=2) year += 2000 ;
    return QDate(year, month, day) ;
}

// The API key expiry date, from the settings store alongside the API key (invalid if not set)
QDate Configuration::getSMSKeyExpiry()
{
    return parseExpiryDate(settings->value("smsapikeyexpires").toString()) ;
}

QString Configuration::getEmailSender()
{
    return ini.get("email-reminder", "sender").trimmed() ;
}

bool Configuration::debugSMSEnabled()
{
    QString m = ini.get("sms-reminder", "debug") ;
    return (m.compare("true", Qt::CaseInsensitive)==0 || m.compare("yes", Qt::CaseInsensitive)==0) ;
}

bool Configuration::debugEmailEnabled()
{
    QString m = ini.get("email-reminder", "debug") ;
    return (m.compare("true", Qt::CaseInsensitive)==0 || m.compare("yes", Qt::CaseInsensitive)==0) ;
}

bool Configuration::encryptedEnabled()
{
    QString m = ini.get("database", "encrypted") ;
    return (m.compare("true", Qt::CaseInsensitive)==0 || m.compare("yes", Qt::CaseInsensitive)==0) ;
}

bool Configuration::calendarSyncEnabled()
{
    QString m = settings->value("calendarsync").toString() ;
    return (m.compare("yes", Qt::CaseInsensitive)==0) ;
}

bool Configuration::contactSyncEnabled()
{
    QString m = settings->value("contactsync").toString() ;
    return (m.compare("yes", Qt::CaseInsensitive)==0) ;
}

bool Configuration::calendarShowBirthdays()
{
    QString m = settings->value("showbirthdays").toString() ;
    return (m.compare("yes", Qt::CaseInsensitive)==0) ;
}

// The appointment window: the days before today, and the days after today, that appointments are kept and shown for.
// Blank or unreadable gives the default: 90 days before, and two years (730 days) after.
int Configuration::calendarWindowBefore()
{
    bool ok = false ;
    int days = settings->value("windowbefore").toString().toInt(&ok) ;
    return (ok && days>=0) ? days : 90 ;
}

int Configuration::calendarWindowAfter()
{
    bool ok = false ;
    int days = settings->value("windowafter").toString().toInt(&ok) ;
    return (ok && days>=0) ? days : 730 ;
}

Encryption* Configuration::encryption()
{
    return enc ;
}

void Configuration::setEncryption(Encryption* enc)
{
    this->enc = enc ;
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
    // Reminders and confirmations are only sent from a database that ContactManager is master of.  With Google as master,
    // a sync can bring back appointments that were messaged before, so they are not sent from here at all.
    if (getDatabaseMaster()==GoogleMaster) return false ;

    QString settingEnableReminders = settings->value("enablereminders").toString() ;
    return (settingEnableReminders.compare("yes")==0) ;
}

// A whole number (of hours or minutes) from the ini file, or the default if it is missing or not a number
static int wholeNumberOrDefault(QString text, int defaultvalue)
{
    bool ok ;
    int number = text.trimmed().toInt(&ok) ;
    return (ok && number>=0) ? number : defaultvalue ;
}

// A time of day (h:mm) from the ini file, or the default if it is missing or not a time
static QTime timeOrDefault(QString text, QTime defaultvalue)
{
    QTime time = QTime::fromString(text.trimmed(), "H:mm") ;
    return time.isValid() ? time : defaultvalue ;
}

int Configuration::getReminderWindowFromHours()
{
    return wholeNumberOrDefault(ini.get("reminders", "windowfromhours"), 2) ;
}

int Configuration::getReminderWindowToHours()
{
    return wholeNumberOrDefault(ini.get("reminders", "windowtohours"), 24) ;
}

// Zero is allowed: a message is then sent on the next check, once the appointment is saved
int Configuration::getWaitBeforeSendMinutes()
{
    return wholeNumberOrDefault(ini.get("reminders", "waitbeforesendmins"), 5) ;
}

// Whether an .ics appointment is attached to reminder and update emails (ini [reminders] attachics=yes/no, default no)
bool Configuration::getAttachICS()
{
    return ini.get("reminders", "attachics").trimmed().compare("yes", Qt::CaseInsensitive)==0 ;
}

QTime Configuration::getSendWindowStart()
{
    return timeOrDefault(ini.get("reminders", "sendfrom"), QTime(7, 0)) ;
}

QTime Configuration::getSendWindowEnd()
{
    return timeOrDefault(ini.get("reminders", "sendto"), QTime(22, 0)) ;
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

void Configuration::on_conflictGoogle_clicked()
{
    ui->enableReminders->setEnabled(false) ;
    ui->enableReminders->setChecked(false) ;
}

void Configuration::on_conflictContactManager_clicked()
{
    ui->enableReminders->setEnabled(true) ;
}

int Configuration::getWaitBeforeUpdateMinutes()
{
    return wholeNumberOrDefault(ini.get("reminders", "waitbeforeupdatemins"), 5) ;
}

int Configuration::getMaxMessagesPerContactPerDay()
{
    return wholeNumberOrDefault(ini.get("reminders", "maxmessagesperdaypercontact"), 10) ;
}

int Configuration::getContactMessagesToday(const QString &contactid, const QString &day)
{
    return settings->value("messagecount/" + contactid + "/" + day, 0).toInt() ;
}

void Configuration::addContactMessageToday(const QString &contactid, const QString &day)
{
    settings->setValue("messagecount/" + contactid + "/" + day, getContactMessagesToday(contactid, day) + 1) ;
}
