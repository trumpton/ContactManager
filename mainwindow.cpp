//
// Contact Manager
//
//  mainwindow.cpp
//
//
// ==========================================================================

#include <QProcess>
#include <QMessageBox>
#include <QCheckBox>
#include <QPushButton>
#include <QShortcut>

#include "mainwindow.h"
#include "transfer.h"
#include "transferfile.h"
#include "sendsmsform.h"
#include "../Lib/itemselect.h"
#include "../Lib/alertsound.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    searchtext="" ;

    gConf=NULL ;
    timer=NULL ;
    enc=NULL ;
    find=NULL ;

    ui->setupUi(this);

    // Enable debug
    setupdebug();

    // Set the application icon
    QIcon icon(":icon-image.png");
    setWindowIcon(icon);

    // Initialise and load configuration
    gConf = new Configuration(this) ;

    // Initialise encryption i/o
    enc = new Encryption(QString("trumpton.uk"), QString("TextFileEncryption")) ;
    gConf->setEncryption(enc);

    // Set up Calendar Model
    calendarlist.clear() ;
    ui->listCalendar->setModel(&calendarlist) ;

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
    logLoadSummary() ;
    calendar.dumpEntries("local", db) ;

    ui->editDateOfBirth->setFormat("", "dd/MMM/yyyy", true) ;
    //populateDialog(SELECT_OVERVIEW) ;

    //on_tabBar_currentChanged(1) ;
    //on_actionGotoToday_triggered() ;

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

    // Connect signals for password menu and calendar list
    // ref: https://stackoverflow.com/questions/26422154/my-signal-slot-connection-does-not-work

    connect(ui->menuKeyandPassword,
            &QMenu::aboutToShow,
            this,
            &MainWindow::refreshPasswordMenu) ;

    connect(ui->listCalendar,
            &SafeListView::selectionChanged,
            this,
            &MainWindow::on_listCalendar_SelectionChanged) ;

    QShortcut* delshortcut = new QShortcut(QKeySequence(Qt::Key_Delete), ui->listCalendar);
    connect(delshortcut,
            &QShortcut::activated,
            this,
            &MainWindow::on_actionRemoveAppointment_triggered) ;

    // Timer ticks every second
    timer = new QTimer(this);
    connect(timer,
            &QTimer::timeout,
            this,
            &MainWindow::tick) ;
    timer->start(1000);

    // Fire 'Goto Today' after initialisation complete
    QTimer::singleShot(500, this, SLOT(on_initial_focus()));

    dbg("Initialisation complete") ;

}

void MainWindow::on_initial_focus()
{
    populateDialog(SELECT_OVERVIEW) ;
    on_tabBar_currentChanged(1) ;
    on_actionGotoToday_triggered() ;
}

void MainWindow::abort()
{
    QApplication::exit() ;
}


// Counts what was loaded, by category, so that large numbers can be traced to
// their cause.  Only counts are logged (no names).  Written as "LOAD SUMMARY" lines.
void MainWindow::logLoadSummary()
{
    // Contacts
    int empty=0, deleted=0, deletedlinked=0, hiddenonly=0, hiddenlinked=0, live=0, livelinked=0 ;
    QHash<QString,int> googleids ;

    for (int i=0; i<db.size(); i++) {
        Contact &c = db.getContact(i) ;
        bool isdeleted = c.isSet(Contact::Deleted) ;
        bool ishidden = c.isSet(Contact::Hidden) ;
        bool linked = !c.getField(Contact::GoogleRecordId).isEmpty() ;

        if (c.isEmpty()) empty++ ;
        if (linked) googleids[c.getField(Contact::GoogleRecordId)]++ ;

        if (isdeleted) {
            deleted++ ;
            if (linked) deletedlinked++ ;
        } else if (ishidden) {
            hiddenonly++ ;
            if (linked) hiddenlinked++ ;
        } else {
            live++ ;
            if (linked) livelinked++ ;
        }
    }

    int sharedlinks=0 ;
    for (auto it=googleids.constBegin(); it!=googleids.constEnd(); ++it) {
        if (it.value()>1) sharedlinks += it.value()-1 ;
    }

    dbg(QString("LOAD SUMMARY contacts: total %1, empty %2, deleted %3 (linked to Google %4), hidden only %5 (linked %6), live %7 (linked %8), extra entries sharing a Google link %9")
        .arg(db.size()).arg(empty).arg(deleted).arg(deletedlinked).arg(hiddenonly).arg(hiddenlinked)
        .arg(live).arg(livelinked).arg(sharedlinks)) ;

    // Calendar
    QDateTime now = QDateTime::currentDateTimeUtc() ;
    QDateTime windowstart = now.addDays(-APPOINTMENTSTARTWINDOW) ;
    QDateTime windowend = now.addDays(APPOINTMENTENDWINDOW) ;

    int cdeleted=0, ccancelled=0, clinked=0, cinwindow=0, cbefore=0, cafter=0 ;
    int cbirthday=0, cinternet=0, ccontact=0, ccontactdeleted=0, ccontactmissing=0 ;

    for (int i=0; i<calendar.size(); i++) {
        Appointment &a = calendar.getAppointment(i) ;
        QDateTime when = a.getDate(Appointment::From) ;

        if (a.isSet(Appointment::Deleted)) cdeleted++ ;
        if (a.getField(Appointment::GoogleStatus).compare("cancelled")==0) ccancelled++ ;
        if (!a.getField(Appointment::GoogleRecordId).isEmpty()) clinked++ ;
        if (when < windowstart) cbefore++ ;
        else if (when > windowend) cafter++ ;
        else cinwindow++ ;
        if (a.getField(Appointment::Summary).contains("birthday", Qt::CaseInsensitive)) cbirthday++ ;
        if (a.isSet(Appointment::InternetOwned)) cinternet++ ;

        QString contactid = a.getField(Appointment::ContactId) ;
        if (!contactid.isEmpty()) {
            ccontact++ ;
            Contact &c = db.getContactById(contactid) ;
            if (c.isNull()) ccontactmissing++ ;
            else if (c.isSet(Contact::Deleted)) ccontactdeleted++ ;
        }
    }

    dbg(QString("LOAD SUMMARY calendar: total %1, deleted %2, cancelled on Google %3, linked to Google %4, "
                "in sync window %5, before window %6, after window %7, summary mentions birthday %8, "
                "read-only (internet) %9, linked to a contact %10 (contact deleted %11, contact missing %12)")
        .arg(calendar.size()).arg(cdeleted).arg(ccancelled).arg(clinked)
        .arg(cinwindow).arg(cbefore).arg(cafter).arg(cbirthday)
        .arg(cinternet).arg(ccontact).arg(ccontactdeleted).arg(ccontactmissing)) ;
}


MainWindow::~MainWindow()
{

    // Save current details if required
    SaveTabs() ;
    play(FileSave) ;
    ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, false);

    // Free up memory
    if (gConf) delete gConf ; gConf=NULL ;
    if (timer) delete timer ; timer=NULL ;
    if (ui) delete ui; ui=NULL ;
    if (enc) delete enc ; enc=NULL ;
    if (find) delete find ; find=NULL ;
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
                dbg("editHistory->setFocus()") ;
                ui->editHistory->setFocus() ;
                break ;
        }

    }

}

// TODO: Fix all tab-specific menu options
void MainWindow::on_tabBar_currentChanged(int index)
{
    if (index==1) { // Calendar
        ui->actionMoveAppointment->setEnabled(true) ;
        ui->actionRemoveAppointment->setEnabled(true) ;
    } else {
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
    SaveTodo() ;
    SaveEditHistory() ;
    SaveCalendarTab();
    SaveContactTab() ;
    LoadOverviewTab() ;
    db.save() ;
    calendar.save() ;
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

    // A blank contact on screen is removed by the sync, so stop showing it first
    if (!db.getSelected().isNull() && db.getSelected().isBlank()) {
        db.selectContact(SELECT_OVERVIEW) ;
    }

    frm = new GoogleUpdateDialog(this) ;
    play(Ok) ;
    frm->doUpdate(google, db, calendar) ;
    delete frm ;

    // Save now, so links cleared by the sync are kept if the app is stopped
    db.save() ;

    // And re-load.  If the selected contact was removed by the sync, show the overview
    if (db.getSelected().isNull()) {
        populateDialog(SELECT_OVERVIEW) ;
    } else {
        LoadTabs() ;
    }
}



void MainWindow::on_actionTidyContacts_triggered()
{
    // Save what is on screen, and stop showing a contact that may be merged away
    SaveTabs() ;
    db.selectContact(SELECT_OVERVIEW) ;

    // Progress is shown in a window with a log, which also carries the summary at the end
    GoogleUpdateDialog progress(this) ;
    progress.showProgress("Tidy Contacts") ;

    // Repeated history lines go first, in every contact
    int historylines=0, historycontacts=0 ;
    historylines = db.tidyHistories(historycontacts) ;
    if (historycontacts>0) {
        progress.reportProgress(0, QString("Removed %1 repeated history lines from %2 contacts").arg(historylines).arg(historycontacts)) ;
        db.save() ;
    }

    // Unnamed entries: blank ones are deleted, and one whose only detail is an email address takes it as
    // its surname.  The others are left, and listed at the end.
    int emptied=0, renamed=0 ;
    QStringList unnamedleft ;
    db.tidyUnnamed(emptied, renamed, unnamedleft) ;
    if (emptied>0 || renamed>0) db.save() ;

    // Lists the merged entry, one detail per line.  Each value is marked with the entry it came from, (1) or (2).
    auto describe = [](Contact &merged, Contact &first, Contact &second) {
        QStringList lines ;

        // The entry (1 or 2) that holds this value in one of the fields, or blank if neither does
        auto origin = [&](QString value, QList<Contact::ContactRecord> fields) -> QString {
            QString v = value.trimmed() ;
            if (v.isEmpty()) return QString() ;
            foreach (Contact::ContactRecord f, fields) if (first.getField(f).trimmed()==v) return "1" ;
            foreach (Contact::ContactRecord f, fields) if (second.getField(f).trimmed()==v) return "2" ;
            return QString() ;
        } ;

        // A line for a value.  Continuation lines are indented to line up under the value.
        auto add = [&](QString label, QString value, QString from) {
            QString v = value.trimmed() ;
            if (v.isEmpty()) return ;
            QString indent(4 + label.size() + 2, ' ') ;
            lines << "    " + label + ": " + v.replace("\n", "\n" + indent) + (from.isEmpty() ? QString() : QString(" (%1)").arg(from)) ;
        } ;

        QList<Contact::ContactRecord> emails = { Contact::Email, Contact::Email2, Contact::Email3, Contact::Email4 } ;
        QList<Contact::ContactRecord> phones = { Contact::Phone, Contact::Work, Contact::Mobile,
                                                 Contact::Phone2, Contact::Phone3, Contact::Phone4 } ;

        // A value with a label of its own: the title, when it has one
        auto field = [&](QString label, Contact::ContactRecord f, QList<Contact::ContactRecord> fields) {
            add(label, merged.getField(f), origin(merged.getField(f), fields)) ;
        } ;
        auto titled = [&](QString label, Contact::ContactRecord f, Contact::ContactRecord titlefield, QList<Contact::ContactRecord> fields) {
            QString title = merged.getField(titlefield).trimmed() ;
            field(title.isEmpty() ? label : title, f, fields) ;
        } ;

        add("Name", QString(merged.getFormattedName(false, true)), origin(merged.getField(Contact::Surname), { Contact::Surname })) ;
        field("Organisation", Contact::Organisation, { Contact::Organisation }) ;
        field("Address", Contact::Address, { Contact::Address }) ;
        field("Address 2", Contact::Address2, { Contact::Address2 }) ;
        field("Birthday", Contact::Birthday, { Contact::Birthday }) ;
        field("Email", Contact::Email, emails) ;
        field("Other", Contact::Email2, emails) ;
        titled("Email 3", Contact::Email3, Contact::Email3Title, emails) ;
        titled("Email 4", Contact::Email4, Contact::Email4Title, emails) ;
        field("Phone", Contact::Phone, phones) ;
        field("Work", Contact::Work, phones) ;
        field("Mobile", Contact::Mobile, phones) ;
        titled("Phone 2", Contact::Phone2, Contact::Phone2Title, phones) ;
        titled("Phone 3", Contact::Phone3, Contact::Phone3Title, phones) ;
        titled("Phone 4", Contact::Phone4, Contact::Phone4Title, phones) ;

        QStringList commentsfrom ;
        if (!first.getField(Contact::Comments).trimmed().isEmpty()) commentsfrom << "1" ;
        if (!second.getField(Contact::Comments).trimmed().isEmpty()) commentsfrom << "2" ;
        add("Comments", merged.getField(Contact::Comments), commentsfrom.join(", ")) ;

        return lines ;
    } ;

    int automatic=0, confirmed=0, declined=0 ;
    QStringList changedlines, skippedlines ;   // reported again at the end
    int total = db.size() ;
    bool stopped = false ;

    for (int i=0; i<total && !stopped; i++) {

        Contact &first = db.getContact(i) ;
        if (first.isSet(Contact::Deleted) || db.isUnnamed(first)) continue ;

        int percent = (i*100)/total ;
        progress.reportProgress(percent, QString("Checking %1").arg(QString(first.getFormattedName(false, true)))) ;

        for (int j=i+1; j<total; j++) {

            if (progress.isAborted()) { stopped=true ; break ; }
            if (first.isSet(Contact::Deleted)) break ;      // merged away by an earlier pair

            Contact &second = db.getContact(j) ;
            if (second.isSet(Contact::Deleted) || db.isUnnamed(second)) continue ;
            if (!db.isMergeCandidate(first, second)) continue ;

            // Keep the visible entry, then the one linked to Google.  If neither differs, keep the older one.
            bool firstlinked = !first.getField(Contact::GoogleRecordId).isEmpty() ;
            bool secondlinked = !second.getField(Contact::GoogleRecordId).isEmpty() ;
            QDateTime firstcreated = first.getDate(Contact::Created) ;
            QDateTime secondcreated = second.getDate(Contact::Created) ;
            bool firsthidden = first.isSet(Contact::Hidden) ;
            bool secondhidden = second.isSet(Contact::Hidden) ;
            bool keepfirst ;
            if (firsthidden != secondhidden) keepfirst = !firsthidden ;
            else if (firstlinked != secondlinked) keepfirst = firstlinked ;
            else keepfirst = !(secondcreated < firstcreated) ;

            Contact &keep = keepfirst ? first : second ;
            Contact &remove = keepfirst ? second : first ;

            Contact merged ;
            QList<MergeConflict> conflicts ;
            db.planMerge(keep, remove, merged, conflicts) ;

            QString namefirst = QString(first.getFormattedName(false, true)) ;
            QString namesecond = QString(second.getFormattedName(false, true)) ;
            QString pair = QString("%1 and %2").arg(namefirst, namesecond) ;

            if (conflicts.isEmpty() && db.isSafeToAutoMerge(first, second)) {
                automatic++ ;
                progress.reportProgress(percent, QString("Merged automatically: %1").arg(pair)) ;
            } else {
                // The entries are numbered (1) and (2) in the order they are listed, with hidden ones marked
                auto entry = [](QString name, bool hidden, int number) {
                    return QString("    %1 (%2%3)").arg(name).arg(number).arg(hidden ? ", hidden" : "") ;
                } ;
                QStringList sections ;
                sections << "These entries may be the same person:\n" + entry(namefirst, firsthidden, 1) + "\n" + entry(namesecond, secondhidden, 2) ;

                // The values that differ, from each entry.  A value with no free slot is said to go to Comments.
                int keepindex = keepfirst ? 1 : 2 ;
                if (conflicts.isEmpty()) {
                    sections << "No details conflict, but they do not share a name, phone number, email address or address, so they may be different people." ;
                } else {
                    QStringList lines ;
                    foreach (const MergeConflict &c, conflicts) {
                        if (c.overflow) {
                            bool fromkept = !c.keptvalue.isEmpty() ;
                            QString value = fromkept ? c.keptvalue : c.removedvalue ;
                            int index = fromkept ? keepindex : 3-keepindex ;
                            lines << QString("    %1(%2): %3, which has no free slot, so it goes to Comments").arg(c.field).arg(index).arg(value.trimmed().replace("\n", ", ")) ;
                            continue ;
                        }
                        QString firstvalue = keepfirst ? c.keptvalue : c.removedvalue ;
                        QString secondvalue = keepfirst ? c.removedvalue : c.keptvalue ;
                        if (!firstvalue.trimmed().isEmpty()) lines << QString("    %1(1): %2").arg(c.field, firstvalue.trimmed().replace("\n", ", ")) ;
                        if (!secondvalue.trimmed().isEmpty()) lines << QString("    %1(2): %2").arg(c.field, secondvalue.trimmed().replace("\n", ", ")) ;
                    }
                    sections << "These details differ:\n" + lines.join("\n") ;
                }

                sections << "If merged, the entry will be:\n" + describe(merged, first, second).join("\n") ;
                sections << "Merge them?" ;

                if (!warningYesNoDialog(this, "Tidy Contacts", sections.join("\n\n"), "Merge", QChar('y'), "Skip", QChar('n'))) {
                    declined++ ;
                    skippedlines << pair ;
                    progress.reportProgress(percent, QString("Not merged (you chose Skip): %1").arg(pair)) ;
                    continue ;
                }
                confirmed++ ;
                progress.reportProgress(percent, QString("Merged after you confirmed: %1").arg(pair)) ;
            }

            // Calendar entries for the removed contact follow the kept one
            QString removedid = remove.getField(Contact::ID) ;
            QString keptid = keep.getField(Contact::ID) ;
            db.applyMerge(keep, remove, merged) ;
            calendar.repointContact(removedid, keptid) ;
            QString keptname = keepfirst ? namefirst : namesecond ;
            QString removedname = keepfirst ? namesecond : namefirst ;
            changedlines << QString("kept and updated: %1 (merged in: %2, now removed)").arg(keptname, removedname) ;
        }
    }

    if (automatic==0 && confirmed==0 && declined==0 && historylines==0 && emptied==0 && renamed==0 && unnamedleft.isEmpty()) {
        progress.finishProgress(stopped ? "Stopped at your request." : "No duplicate contacts, repeated history lines or unnamed entries were found.") ;
        return ;
    }

    // Removed entries that are not on Google go to the deleted folder now.  The others follow at the next sync.
    int linked=0 ;
    int archived = db.tidyDeleted(linked) ;
    calendar.save() ;
    populateDialog(SELECT_OVERVIEW) ;

    QStringList summary ;
    summary << QString("Removed %1 repeated history lines from %2 contacts.").arg(historylines).arg(historycontacts) ;
    summary << QString("Unnamed entries: %1 empty entries deleted, %2 given their email address as surname.").arg(emptied).arg(renamed) ;
    if (!unnamedleft.isEmpty()) {
        summary << QString("Unnamed entries left alone (%1):").arg(unnamedleft.size()) ;
        foreach (const QString &line, unnamedleft) summary << "    " + line ;
    }
    summary << QString("Summary: merged %1 automatically and %2 after you confirmed; %3 not merged (you chose Skip).")
               .arg(automatic).arg(confirmed).arg(declined) ;
    summary << QString("Moved %1 removed entries to the deleted folder.").arg(archived) ;
    if (linked>0) summary << QString("%1 removed entries are still on Google.").arg(linked) ;
    summary << "Run Google Sync to send the merged details, and to remove the others from Google." ;
    if (stopped) summary << "Stopped at your request, so the remaining contacts were not checked." ;

    summary << QString("Records changed by this merge (%1):").arg(changedlines.size()) ;
    foreach (const QString &line, changedlines) summary << "    " + line ;
    summary << QString("Skipped at your choice (%1):").arg(skippedlines.size()) ;
    foreach (const QString &line, skippedlines) summary << "    " + line ;
    progress.finishProgress(summary.join("\n")) ;
}

// Saves the records to a file chosen by the user, as XML or as CSV
static void exportToFile(QWidget *parent, const QList<TransferRecord> &records, QString title)
{
    QString selected ;
    QString fileName = QFileDialog::getSaveFileName(parent, title, QDir::homePath(),
                                                    "XML Files (*.xml);;CSV Files (*.csv)", &selected) ;
    if (fileName.isEmpty()) return ;

    // The extension decides the format.  Without one, the filter chosen does.
    bool asXml = selected.startsWith("XML") ;
    if (fileName.endsWith(".xml", Qt::CaseInsensitive)) asXml = true ;
    else if (fileName.endsWith(".csv", Qt::CaseInsensitive)) asXml = false ;
    else fileName += asXml ? ".xml" : ".csv" ;

    QString error = writeTransferFile(fileName, records, asXml) ;
    if (!error.isEmpty()) {
        warningOkDialog(parent, title, error, true) ;
        return ;
    }
    warningOkDialog(parent, title, QString("%1 records were exported to %2.").arg(records.size()).arg(fileName)) ;
}

void MainWindow::on_actionExportContacts_triggered()
{
    db.sort() ;
    exportToFile(this, contactTransferRecords(db), "Export Contacts") ;
}

void MainWindow::on_actionExportAppointments_triggered()
{
    calendar.sort() ;
    exportToFile(this, appointmentTransferRecords(calendar), "Export Appointments") ;
}

// Asks whether an existing record is to be overwritten.  Returns true to overwrite.  'dontask' is set when
// "Don't ask again" is ticked and a button is pressed.
static bool askOverwrite(QWidget *parent, const QString &description, bool &dontask)
{
    QMessageBox box(parent) ;
    box.setWindowTitle("Import") ;
    box.setIcon(QMessageBox::Question) ;
    box.setText(QString("%1 already exists.").arg(description)) ;
    box.setInformativeText("Overwrite it with the imported details?") ;
    QPushButton *overwrite = box.addButton("Overwrite", QMessageBox::AcceptRole) ;
    QPushButton *skip = box.addButton("Skip", QMessageBox::RejectRole) ;
    box.setDefaultButton(skip) ;
    QCheckBox *again = new QCheckBox("Don't ask again for the rest of this import", &box) ;
    box.setCheckBox(again) ;

    box.exec() ;
    dontask = again->isChecked() && box.clickedButton()!=nullptr ;
    return box.clickedButton()==overwrite ;
}

void MainWindow::on_actionImport_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Import", QDir::homePath(), "Export Files (*.xml *.csv)") ;
    if (fileName.isEmpty()) return ;

    QList<TransferRecord> records ;
    QString error = readTransferFile(fileName, records) ;
    if (!error.isEmpty()) {
        warningOkDialog(this, "Import", error, true) ;
        return ;
    }
    if (records.isEmpty()) {
        warningOkDialog(this, "Import", "The file has no records to import.") ;
        return ;
    }

    SaveTabs() ;
    db.selectContact(SELECT_OVERVIEW) ;

    int added=0, overwritten=0, skipped=0, unknown=0 ;

    // Set by "Don't ask again": 0 asks about each existing record, 1 overwrites them all, -1 skips them all
    int policy = 0 ;

    for (const TransferRecord &rec : records) {
        bool iscontact = (rec.kind==QLatin1String("contact")) ;
        bool isappointment = (rec.kind==QLatin1String("appointment")) ;
        if (!iscontact && !isappointment) {
            unknown++ ;
            continue ;
        }

        bool exists = iscontact ? contactExists(db, rec) : appointmentExists(calendar, rec) ;
        if (exists) {
            bool dooverwrite ;
            if (policy!=0) {
                dooverwrite = (policy==1) ;
            } else {
                bool dontask = false ;
                dooverwrite = askOverwrite(this, transferDescription(rec), dontask) ;
                if (dontask) policy = dooverwrite ? 1 : -1 ;
            }

            if (!dooverwrite) {
                skipped++ ;
                continue ;
            }
            if (iscontact) importContact(db, rec, true) ;
            else importAppointment(calendar, rec, true) ;
            overwritten++ ;
            continue ;
        }

        if (iscontact) importContact(db, rec, false) ;
        else importAppointment(calendar, rec, false) ;
        added++ ;
    }

    db.save() ;
    calendar.save() ;
    populateDialog(SELECT_OVERVIEW) ;

    QStringList summary ;
    summary << QString("Added %1 records.").arg(added) ;
    summary << QString("Overwritten %1 existing records.").arg(overwritten) ;
    summary << QString("Skipped %1 existing records.").arg(skipped) ;
    if (unknown>0) summary << QString("%1 records were not contacts or appointments, and were not imported.").arg(unknown) ;
    summary << "Run Google Sync to send new and changed contacts to Google." ;
    warningOkDialog(this, "Import", summary.join("\n")) ;
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
    for (int i=0, sz=db.size(); i<sz; i++) {
        Contact& c=db.getContact(i) ;
        if (!c.isSet(Contact::Deleted) && !c.isSet(Contact::Hidden)) {
            QString name = c.getFormattedName() ;
            QString data = c.getField(Contact::ID) ;
            list.addEntry(name, data) ;
        }
    }

    // Added after the sorted contacts, at the top, so that it stays first
    list.addEntry("-- Add Contact --", SELECT_ADDENTRY, 0) ;

    list.selectData(thisid) ;
    if (list.index()<=0) list.setIndex(1) ;

    play(Query) ;
    bool result = list.exec() ;
    if (result) {
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
    QStringList args = cmd.split( QRegularExpression(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)") );
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
        ui->editHistory->setAccessibleDescription("History") ;
        ui->editToDo->setAccessibleDescription("To Do List") ;
    } else {
        setWindowTitle(name + " - Contact Manager") ;
        ui->editOverview->setAccessibleDescription("Overview for " + name) ;
        ui->editHistory->setAccessibleDescription("History for " + name) ;
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

