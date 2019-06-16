//
// TODO: After Control-S, the focus on the braille display is returned to the Calendar
//       but the visible entry remains on the contact details
//
// TODO: Braille display does not speak calendar entries on startup
//       Suspect that the code to force the focus to change on the calendar tab is buggering up the refresh
//       Don't need to LoadTabs the Calendar Tab on Control-S ???
//
// TODO: safe save
//
// TODO: check for appointment clashes on startup
//
// TODO: Email address ends up being copied into home, even if just work is specified.
//
// TODO: Control-S after editing a new contact saves it but doesn't update the menus etc.
//
// TODO: A Google appointment is downloaded, and then re-uploaded as a different appointment.
//       It never syncs properly.  Even if you delete the google appointments, each sync re-uploads
//       the same ones agains and again.
//
// TODO:  Some google uploads gets error 400 - probably due to invalid format DOB
//


// ==========================================================================
//
// There are lots of things to do
//
//  The big change in moving contacts to the people api:
//
//    You can now be a member of several groups
//    You can read the groups when reading a contact
//    You have to write to the groups api to change groupies though
//    Ive changed syncing so that a cache of what was on google is not maintained
//    So syncing will now have to overwrite a whole contact (at one end or the other) not just individual fields
//    Need to draw all this out on a bit of paper
//
// ==========================================================================

#include <QProcess>
#include <QMessageBox>
#include "mainwindow.h"
#include "sendsmsform.h"
#include "../Lib/itemselect.h"
#include "../Lib/alertsound.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    calendarlist.clearStrings() ;
    searchtext="" ;

    ui->setupUi(this);

    // Enable debug
    setupdebug();

    // Connect signals for password menu
    connect(ui->menuKeyandPassword, SIGNAL(aboutToShow()), this, SLOT(refreshPasswordMenu())) ;

    // Set the application icon
    QIcon icon(":icon-image.png");
    setWindowIcon(icon);

    // Initialise and load configuration
    gConf = new Configuration(this) ;

    // Initialise encryption i/o
    enc = new Encryption(QString("trumpton.uk"), QString("TextFileEncryption")) ;
    gConf->setEncryption(enc);

    // Timer ticks every second
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
    timer->start(1000);

    // Populate contact record with a blank entry
    db.selectContact(SELECT_OVERVIEW);

    gSavePath = gConf->getDatabasePath() ;
    gContactSavePath = gSavePath + "contact" + QDir::toNativeSeparators("/")  ;
    gCalendarSavePath = gSavePath + "calendar" + QDir::toNativeSeparators("/")  ;

    // Set the Default Codecs for Strings
    QTextCodec::setCodecForLocale(QTextCodec::codecForName(gConf->getCodec().toLatin1().constData()));

    // Login if not already done so
    int count=0 ;
    if (!enc->loggedIn()) do {
        dbg("Logging In") ;
        enc->login() ;
     } while (!enc->loggedIn() && ++count<3) ;

     // Login failed, so set key
     while (!enc->loggedIn()) {
         if (QMessageBox::question(this, "Contact Manager", "You have failed to login.  To set or reset the key, select Yes, and otherwise select Close to exit the program", QMessageBox::Yes|QMessageBox::Close)==QMessageBox::Yes) {
             dbg("Login Complete") ;
             enc->setKey();
         } else {
             // Calling abort from Constructor doesn't work - it has to be from a function initiated with a signal
             dbg("Sending abort signal, login failed") ;
             QTimer::singleShot(1, this, SLOT(abort())) ;
             return ;
         }
     }


    // Load the local data
    dbg("Loading database and calendar") ;
    db.load() ;
    calendar.load() ;

    ui->editDateOfBirth->setFormat("", "dd/MMM/yyyy", true) ;
    populateDialog(SELECT_OVERVIEW) ;

    on_tabBar_currentChanged(1) ;
    on_actionGotoToday_triggered() ;

    // Warn if no synchronisation has taken place in a while
    bool changed=false ;
    QDateTime lastcalendarsync = isoStringToDateTime(gConf->getLastGoogleCalendarSyncDate()) ;
    QDateTime lastcontactsync = isoStringToDateTime(gConf->getLastGoogleContactSyncDate()) ;
    for (int i=0; i<db.size() && !changed; i++) {
        if (db.getContact(i).getDate(Contact::Updated) > lastcontactsync)
            changed=true ;
    }
    for (int i=0; i<calendar.size() && !changed; i++) {
        if (calendar.getAppointment(i).getDate(Appointment::Updated) > lastcalendarsync)
            changed=true ;
    }
    if (changed) {
        QDateTime now = QDateTime::currentDateTimeUtc() ;
        if (lastcalendarsync > now.addDays(14) || lastcontactsync > now.addDays(14)) {
            warningOkDialog(this, "Contact Manager Warning", "You have made changes to contacts / calendar, and it is some time since you synchronised with google.") ;
        }
    }

    // Tidy up old log files
    dbg("Tidying old log files") ;
    google.resetLogFiles();

    // Setup Advanced Find
    find = new AdvancedFind(this) ;

    // Set up Calendar Model
    ui->listCalendar->setModel(calendarlist.getModel()) ;


    dbg("Initialisation complete") ;

}

void MainWindow::abort()
{
    QApplication::exit() ;
}


MainWindow::~MainWindow()
{
    // Save current details if required
    SaveTabs() ;
    play(FileSave) ;
    ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, false);

    if (gConf) delete gConf ;
    if (ui) delete ui;
    if (enc) delete enc ;
    if (find) delete find ;
}

void MainWindow::refreshPasswordMenu()
{
    bool loggedin = enc->loggedIn() ;
    ui->actionSetEncryptionKey->setEnabled(!loggedin) ;
    ui->actionChangePassword->setEnabled(!loggedin) ;
    ui->actionLogout->setEnabled(loggedin) ;
}

////////////////////////////////////////////////////////////////////////////////
// Contact Navigation


//
// RefreshMenus
//
//
void MainWindow::refreshMenus(Contact &contact, bool isediting, bool isnew, bool resetcursorpos)
{
    if (contact.isNull()) {

        // With no Contact selected, only the Calendar is accessible
        ui->tabBar->setTabEnabled(OVERVIEWTABPOS, false);
        ui->tabBar->setTabEnabled(CALENDARTABPOS, true);
        ui->tabBar->setTabEnabled(TODOTABPOS, false);
        ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, false);
        ui->tabBar->setTabEnabled(HISTORYTABPOS, false);

        // Enable appropriate menus
        ui->actionDeleteContact->setEnabled(false) ;
        ui->actionEnableContactDetailsEdit->setEnabled(false) ;
        ui->actionEnableEditHistoryEdit->setEnabled(false) ;
        ui->actionGotoJournal->setEnabled(false) ;
        ui->actionGotoToDoList->setEnabled(false) ;
        ui->actionNewAppointment->setEnabled(false) ;
        ui->actionNewMyAppointment->setEnabled(true) ;
        ui->action_Insert_Appointment_After->setEnabled(false) ;
        ui->actionInsert_My_Appointment_After->setEnabled(true) ;
        ui->actionIncome->setEnabled(false) ;
        ui->actionPayment->setEnabled(false) ;
        ui->action_Email_Contact->setEnabled(false) ;
        ui->action_SMS_Contact->setEnabled(false) ;

    } else if (isnew) {

        // New Entry (not yet saved)

        // Enable Tabs
        ui->tabBar->setTabEnabled(OVERVIEWTABPOS, false);
        ui->tabBar->setTabEnabled(CALENDARTABPOS, true);
        ui->tabBar->setTabEnabled(TODOTABPOS, false);
        ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, true);
        ui->tabBar->setTabEnabled(HISTORYTABPOS, false);

        // Enable appropriate menus
        ui->actionDeleteContact->setEnabled(false) ;
        ui->actionEnableContactDetailsEdit->setEnabled(true) ;
        ui->actionEnableEditHistoryEdit->setEnabled(false) ;
        ui->actionGotoJournal->setEnabled(false) ;
        ui->actionGotoToDoList->setEnabled(false) ;
        ui->actionIncome->setEnabled(false) ;
        ui->actionPayment->setEnabled(false) ;
        ui->actionNewAppointment->setEnabled(false) ;
        ui->actionNewMyAppointment->setEnabled(true) ;
        ui->action_Insert_Appointment_After->setEnabled(false) ;
        ui->actionInsert_My_Appointment_After->setEnabled(true) ;
        ui->action_Email_Contact->setEnabled(false) ;
        ui->action_SMS_Contact->setEnabled(false) ;

    } else {

        // Contact Selected / Opened

        // Enable Tabs
        ui->tabBar->setTabEnabled(OVERVIEWTABPOS, true);
        ui->tabBar->setTabEnabled(CALENDARTABPOS, true);
        ui->tabBar->setTabEnabled(TODOTABPOS, true);
        ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, isediting);
        ui->tabBar->setTabEnabled(HISTORYTABPOS, true);

        // Enable appropriate menus
        ui->actionDeleteContact->setEnabled(true) ;
        ui->actionEnableContactDetailsEdit->setEnabled(true) ;
        ui->actionEnableEditHistoryEdit->setEnabled(true) ;
        ui->actionGotoJournal->setEnabled(true) ;
        ui->actionGotoToDoList->setEnabled(true) ;
        ui->actionIncome->setEnabled(true) ;
        ui->actionPayment->setEnabled(true) ;
        ui->actionNewAppointment->setEnabled(true) ;
        ui->actionNewMyAppointment->setEnabled(true) ;
        ui->action_Insert_Appointment_After->setEnabled(true) ;
        ui->actionInsert_My_Appointment_After->setEnabled(true) ;
        ui->action_Email_Contact->setEnabled(true) ;
        ui->action_SMS_Contact->setEnabled(true) ;

    }

    if (resetcursorpos) {

        // Move to the selected tab
        if (contact.isNull()) {
            // No Contact
            if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
                dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
                ui->tabBar->setCurrentIndex(CALENDARTABPOS) ;
            }
            dbg("listCalendar->setFocus()") ;
            ui->listCalendar->setFocus() ;
        } else if (isnew) {
            // Create
            if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
                dbg("tabBar->setCurrentIndex(CONTACTDETAILSTABPOS)") ;
                ui->tabBar->setCurrentIndex(CONTACTDETAILSTABPOS);
            }
            dbg("editFirstName->setFocus()") ;
            ui->editFirstName->setFocus() ;
        } else {
            // Open
            if (ui->tabBar->currentIndex()!=OVERVIEWTABPOS) {
                dbg("tabBar->setCurrentIndex(OVERVIEWTABPOS)") ;
                ui->tabBar->setCurrentIndex(OVERVIEWTABPOS) ;
            }
            dbg("editOverview->setFocus()") ;
            ui->editOverview->setFocus() ;
        }

    }

    QString name = contact.getFormattedName(false, false) ;
    setAccessibleTextAndWindowTitle(name) ;

}

//
// PopulateDialog
//
// Populate all of the dialog fields, and enable the
// appropriate tabs.  Index is the index in the drop-down
// contacts menu.
//
void MainWindow::populateDialog(QString id, bool switchtooverview)
{
    Contact& dro = db.getSelected() ;
    QString oldid = dro.getField(Contact::ID) ;

    // Save current contact & disable the editing tab
    SaveTabs() ;

    // If the contact has changed, force a switch to the overview tab
    int currentindex = ui->tabBar->currentIndex() ;
    bool contactdetailstabselected = (currentindex == CONTACTDETAILSTABPOS) ;
    bool contactchanged = (!id.isEmpty() && oldid.compare(id)!=0) ;
    if (!contactdetailstabselected || contactchanged) {
        ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, false);
    }

    if (contactchanged) {
        switchtooverview=true ;
    }

    if (id.compare(SELECT_OVERVIEW)==0) {

        // No Contact Selected

        // -- Select Contact --
        db.selectContact(id) ;
        LoadTabs() ;

        // And refresh the menus
        refreshMenus(db.getSelected(), false, false, true) ;

        // Goto Today
        on_actionGotoToday_triggered() ;

    } else if (id.compare(SELECT_ADDENTRY)==0) {

        createContact() ;

    } else {

        // Open Contact
        play(FileOpen) ;

        db.selectContact(id) ;
        LoadTabs() ;

        refreshMenus(db.getSelected(), false, false, true) ;

        if (switchtooverview) {
            Contact &c = db.getSelected() ;
            msg(QString("Switching contact to ") + c.getFormattedName(false,false) + QString(" (") + c.getField(Contact::ID) + QString(")")) ;
            currentindex=OVERVIEWTABPOS ;
        }

        if (ui->tabBar->currentIndex()!=currentindex) {
            dbg("tabBar->setCurrentIndex()") ;
            ui->tabBar->setCurrentIndex(currentindex) ;
        }

        switch (currentindex) {
            case OVERVIEWTABPOS:
                dbg("editOverview->setFocus()") ;
                ui->editOverview->setFocus() ;
                break ;
            case CALENDARTABPOS:
                dbg("listCalendar->setFocus()") ;
                ui->listCalendar->setFocus() ;
                break ;
            case TODOTABPOS:
                dbg("editToDo->setFocus()") ;
                ui->editToDo->setFocus() ;
                break ;
            case CONTACTDETAILSTABPOS:
                break ;
            case HISTORYTABPOS:
                dbg("editNotes->setFocus()") ;
                ui->editNotes->setFocus() ;
                break ;
        }

    }

}

// TODO: Fix all tab-specific menu options
void MainWindow::on_tabBar_currentChanged(int index)
{
    if (index==1) { // Calendar
        // Button Alt-N conflicts with Contact Details Alt-N, so only one can be enabled at once
        ui->newAppointmentButton->setEnabled(true) ;
        ui->actionMoveAppointment->setEnabled(true) ;
        ui->actionRemoveAppointment->setEnabled(true) ;
    } else {
        ui->newAppointmentButton->setEnabled(false) ;
        ui->actionMoveAppointment->setEnabled(false) ;
        ui->actionRemoveAppointment->setEnabled(false) ;
    }

    if (index==3) { // Contact Details
        ui->menuGoto_Contact_Details->setEnabled(true) ;
        ui->actionE_Mail->setEnabled(true) ;
        ui->action_Name->setEnabled(true) ;
        ui->action_Address->setEnabled(true) ;
        ui->action_Phone->setEnabled(true) ;
        ui->action_Birthday->setEnabled(true) ;
    } else {
        ui->menuGoto_Contact_Details->setEnabled(false) ;
        ui->actionE_Mail->setEnabled(false) ;
        ui->action_Name->setEnabled(false) ;
        ui->action_Address->setEnabled(false) ;
        ui->action_Phone->setEnabled(false) ;
        ui->action_Birthday->setEnabled(false) ;
    }
}


//
// File I/O functions
//


void MainWindow::LoadTabs()
{
    if (db.getSelected().isSet(Contact::Deleted)) {
        // Select a blank entry, and refresh the screen / menus
        db.selectContact(SELECT_OVERVIEW);
        populateDialog(SELECT_OVERVIEW) ;
        dbg("Contact has been deleted - de-selecting") ;
    }

    LoadOverviewTab() ;
    LoadContactTab() ;
    LoadCalendarTab() ;
    LoadEditHistory() ;
    LoadTodo() ;
}

void MainWindow::SaveTabs()
{
    SaveContactTab() ;
    SaveEditHistory() ;
    SaveTodo() ;
    db.save() ;
    calendar.save() ;
    LoadOverviewTab() ;
}

//
// Menu Handlers
//

// FILE MENU

void MainWindow::on_actionSave_triggered()
{
    // TODO: If the contact is a new one, at the moment, it has to be re-opened
    // once saved.  There is something open does that this does not - something to
    // do with enabling menus?

    msg("Saving Contact ...") ;
    SaveTabs() ;
    LoadOverviewTab() ;
    LoadContactTab() ;
    LoadEditHistory() ;
    LoadTodo() ;

    // Refresh menus, just in case the contact is a new one
    int isediting = ui->tabBar->isTabEnabled(CONTACTDETAILSTABPOS) ;
    refreshMenus(db.getSelected(), isediting, false, false) ;

    play(FileSave) ;
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::quit() ;
}

void MainWindow::on_actionSetup_triggered()
{
    gConf->SetupForm(&db, &google) ;
    play(Query) ;
    gConf->exec() ;
    gConf->SaveForm() ;
}


//
// on_actionGoogleSync_triggered
//
//

void MainWindow::on_action_Google_Sync_triggered()
{
    class GoogleUpdateDialog *frm ;

    // Save tabs before sync
    SaveTabs() ;

    frm = new GoogleUpdateDialog(this) ;
    play(Ok) ;
    frm->doUpdate(google, db, calendar) ;
    delete frm ;

    // And re-load
    LoadTabs() ;
}



void MainWindow::on_actionExportContacts_triggered()
{
    QFileDialog fd ;
    QString dir=QDir::homePath() ;
    play(Query) ;
    QString fileName = fd.getSaveFileName(this, tr("Contacts List File"),
                               dir,
                               tr("Text Files (*.txt)"));

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // TODO: Report file open error
        return ;
    }

    // TODO: update working directory from fd.

    QTextStream out(&file);
    out << "Contacts Database\n\n" ;

    msg("Exporting ....") ;
    db.sort() ;
    for (int i=0; i<db.size(); i++) {
        Contact& c = db.getContact(i) ;
        if (!c.isSet(Contact::Deleted) && !c.isSet(Contact::Hidden)) {
            out << c.getOverview(Contact::contactAsText) << "--------------------------------\n";
        }
    }
    file.close() ;
    msg("Exporting complete.") ;
}

void MainWindow::on_actionExportAppointments_triggered()
{

}

void MainWindow::on_actionShow_Log_triggered()
{
    LogForm form ;
    form.setText(getLog()) ;
    play(Ok) ;
    form.exec() ;
}


void MainWindow::msg(QString msg)
{
    ui->statusBar->showMessage(msg, 6000) ;
}


void MainWindow::on_actionOpen_triggered()
{
    ItemSelect list ;
    QString thisid=db.getSelected().getField(Contact::ID) ;

    list.setTitle("Contact Manager Open", "Load Contact");
    list.addEntry("-- Add Contact --", SELECT_ADDENTRY) ;
    for (int i=0, sz=db.size(); i<sz; i++) {
        Contact& c=db.getContact(i) ;
        if (!c.isSet(Contact::Deleted) && !c.isSet(Contact::Hidden)) {
            QString name = c.getFormattedName() ;
            QString data = c.getField(Contact::ID) ;
            list.addEntry(name, data) ;
        }
    }

    list.selectData(thisid) ;
    if (list.index()<=0) list.setIndex(1) ;

    play(Query) ;
    bool result = list.exec() ;
    if (result) {
        QString name=list.getSelection() ;
        QString data=list.getData() ;
         populateDialog(data) ;
    }
}


void MainWindow::on_actionGotoJournal_triggered()
{
    Contact &c = db.getSelected() ;
    if (c.isNull()) {
        play(NotFound) ;
     } else {
        QString file = gConf->getJournalPath(c.getFormattedName(false, false)) ;
        QString app = gConf->getEasyNotepadPath() ;
        if (app.isEmpty() || file.isEmpty()) {
            errorOkDialog(this, "Error, Not Configured", "EasyNotepad path or Journal folder not configured") ;
        } else {
            QStringList args ;
            args.append(file) ;
            QProcess *myProcess = new QProcess(NULL) ;
            myProcess->start(app, args) ;
        }
    }
}

// Tick triggered every second
void MainWindow::tick()
{
    static int everyhour=1800 ; // First backup occurs in 30 minutes
    static int everyday=86400 ;

    if ((++everyhour)>=3600) {
        everyhour=0 ;
        doBackup() ;
    }

    if (gConf->getEnableReminders()) {

         if ((++everyday)>86400) {
            everyday=0 ;
            int balance = GetSMSBalance() ;
            addLog(QString("SMS Balance = ") + QString::number(balance)) ;
            if (balance>=0 && balance<100) {
                warningOkDialog(this,"Send SMS", "You have low SMS credit") ;
            }
        }

        sendMailSMS() ;
    }
}


void MainWindow::on_action_Email_Contact_triggered()
{
    Contact &c = db.getSelected() ;

    QString email, name ;

    email = c.getField(Contact::Email) ;
    if (email.isEmpty()) email = c.getField(Contact::Email2) ;
    name = c.getFormattedName() ;

    if (email.isEmpty()) {
        warningOkDialog(this, "Email Not Found", "The current contact does not have an email address") ;
        return ;
    }

    QString cmd = gConf->getEmailCommand(name, email) ;
    if (cmd.isEmpty()) {
        errorOkDialog(this, "Error, Not Configured", "Mail program not configured") ;
        return ;
    }

    QProcess *myProcess = new QProcess(NULL) ;
    QStringList args = cmd.split( QRegExp(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)") );
    for (int a=0; a<args.length(); a++) {
      QString entry = args.at(a) ;
      entry = entry.replace("\\\"", "***REALQUOTE***") ;
      entry = entry.replace("\"", "") ;
      entry = entry.replace("***REALQUOTE***", "\\\"") ;
      args.replace(a, entry) ;
    }

    QString app = args.at(0) ;
    args.removeAt(0) ;

    myProcess->start(app, args) ;

    // TODO: Check for error launching

}

void MainWindow::on_action_SMS_Contact_triggered()
{
    Contact& c = db.getSelected() ;
    QString mobile = c.getField(Contact::Mobile) ;

    if (c.isNull() || mobile.isEmpty()) {
        warningOkDialog(this, "Unable to Send", "No contact is selected, or the current contact does not have a mobile") ;
    }
    SendSMSForm smsform ;
    smsform.setRecipient(c.getFormattedName(false, true) + QString(" (") + c.getField(Contact::Mobile) + QString(")")) ;
    play(Query) ;
    if (smsform.exec()) {
        // TODO: from should be gConf->getSMSFrom() ;
        QString from = QString("") ;
        QString message = smsform.getMessage() ;
        if (message.isEmpty()) {
            warningOkDialog(this, "Unable to Send", "No Message Provided") ;
        } else {
            SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
            bool success = sms.send(mobile, from, message) ;
            int balance = sms.getBalance() ;
            if (!success) {
                if (balance>=0 && balance<30) {
                    warningOkDialog(this, "Unable to Send", "Low Balance / Out of Credit.") ;
                } else {
                    warningOkDialog(this, "Unable to Send", "Send Message Failed.") ;
                }
            } else {
                if (balance<0) {
                    warningOkDialog(this, "Message Sent", "Message sent successfully.  Unable to get balance.") ;
                } else {
                    warningOkDialog(this, "Message Sent", "Message sent successfully. Balance = £" + QString::number(balance/100) + QString(".") + QString::number(balance%100) ) ;
                }
                c.getHistory().addEntry(QString("Sent SMS: ") + message) ;
                LoadEditHistory() ;
            }
        }
    }
}


void MainWindow::setAccessibleTextAndWindowTitle(QString name)
{
    dbg(QString("setAccessibleTextAndWindowTitle(%1)").arg(name)) ;

    // Setup the accessible text, window name etc, to include the current contact
    if (name.isEmpty()) {
        setWindowTitle("Contact Manager") ;
        ui->editOverview->setAccessibleDescription("Overview") ;
        ui->editNotes->setAccessibleDescription("History") ;
        ui->editToDo->setAccessibleDescription("To Do List") ;
    } else {
        setWindowTitle(name + " - Contact Manager") ;
        ui->editOverview->setAccessibleDescription("Overview for " + name) ;
        ui->editNotes->setAccessibleDescription("History for " + name) ;
        ui->editToDo->setAccessibleDescription("To Do List for " + name) ;
    }
    dbg("title set complete") ;
}


void MainWindow::createContact()
{  
    // Add new contact
    play(FileOpen) ;

    // -- Add and select Contact --
    db.selectContact(db.addContact()) ;
    LoadTabs() ;

    // Enable the menus
    refreshMenus(db.getSelected(),true,true,true) ;

}

void MainWindow::on_actionNewContact_triggered()
{
    createContact() ;
}

//
// Password Functions
//

void MainWindow::on_actionSetEncryptionKey_triggered()
{
    if (enc) enc->setKey();
}

void MainWindow::on_actionChangePassword_triggered()
{
    if (enc) enc->changePassword();
}

void MainWindow::on_actionLogout_triggered()
{
    if (enc) enc->logout();
}

