#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include <QTimer>
#include "contactdatabase.h"
#include "calendar.h"
#include "configuration.h"
#include "googleaccess.h"
#include "../Lib/accessiblestringlistmodel.h"

#include "ui_mainwindow.h"
#include "../Lib/search.h"
#include "about.h"
#include "contactdatabase.h"
#include "calendar.h"
#include "appointmentform.h"
#include "googleupdatedialog.h"
#include "configuration.h"
#include "transactionform.h"
#include "../Lib/supportfunctions.h"
#include "../Lib/encryption.h"
#include "advancedfind.h"

#include "logform.h"

#include "email.h"
#include "sms.h"

#include <QFileDialog>
#include <QTextStream>
#include <QInputDialog>
#include <QTextCodec>
#include <QPointer>



// Tabs in the main window and their position
#define OVERVIEWTABPOS 0
#define CALENDARTABPOS 1
#define TODOTABPOS 2
#define CONTACTDETAILSTABPOS 3
#define HISTORYTABPOS 4

// Pre-defined entries for the contact
#define SELECT_OVERVIEW "O"
#define SELECT_ADDENTRY "A"

namespace Ui {
class MainWindow;
}

class QLockFile ;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
private slots:

    void abort() ;
    void on_actionSave_triggered();
    void on_actionSetup_triggered();
    void on_actionExit_triggered();
    void on_actionGlobalFind_triggered();

//    void on_actionExportText_triggered();

    void on_actionGotoMe_triggered();
    void on_actionGotoToday_triggered();
    void on_actionGotoDate_triggered();

    void on_actionNewMyAppointment_triggered();
    void on_actionNewAppointment_triggered();
    void on_actionInsert_My_Appointment_After_triggered();
    void on_action_Insert_Appointment_After_triggered();
    void on_actionMoveAppointment_triggered();
    void on_actionRemoveAppointment_triggered();

    void on_actionEnableContactDetailsEdit_triggered();
    void on_actionDeleteContact_triggered();
    void on_actionAbout_triggered();
    void on_actionFind_triggered();
    void on_actionFindNext_triggered();
    void on_action_Google_Sync_triggered();
    void on_actionTidyContacts_triggered();
    void on_actionPayment_triggered();
    void on_actionIncome_triggered() ;
    void on_actionTransactionReport_triggered() ;
    void on_actionManual_triggered();
    void on_listCalendar_activated(const QModelIndex &index);
    void on_actionExportContacts_triggered();
    void on_actionExportAppointments_triggered();
    void on_actionImport_triggered();
    void on_actionShow_Log_triggered();
    void on_tabBar_currentChanged(int index);
    void on_action_Email_Contact_triggered();
    void on_action_SMS_Contact_triggered();

    void on_actionTest_Function_triggered();
    void on_actionOpen_triggered();

    void on_actionEnableEditHistoryEdit_triggered();

    void on_actionGotoMyToDoList_triggered();
    void on_actionGotoToDoList_triggered();

    // Load Journal
    void on_actionGotoJournal_triggered();

    // Navigate Details
    void on_action_Name_triggered();
    void on_actionE_Mail_triggered();
    void on_action_Address_triggered();
    void on_action_Phone_triggered();
    void on_action_Birthday_triggered();

    void tick() ;

    void on_actionNewContact_triggered();

    void on_listCalendar_clicked(const QModelIndex &index);
    void on_listCalendar_SelectionChanged(const QModelIndex &index) ;

    void on_actionSearch_EmailSMSToContacts_triggered();

    void on_actionSearch_EmailSMSToNonClients_triggered();

    void on_actionSearch_UncategorisedContacts_triggered();

    void on_actionSearch_DuplicateContacts_triggered();

    void on_actionSearch_HiddenEmailDuplicates_triggered();

    void on_actionSetEncryptionKey_triggered();

    void on_actionChangePassword_triggered();

    void on_actionLogout_triggered();

    void refreshPasswordMenu() ;


    void on_action_Advanced_Find_triggered();

    void on_initial_focus() ;
    void reportClashes() ;      // Lists the appointments that overlap, when the window opens.  Nothing is changed

private:
    Ui::MainWindow *ui;
    class Calendar calendar ;                       // Calendar
    class ContactDatabase db ;                      // Database of Everything
    QString searchtext ;                            // search text for find
    GoogleAccess google ;                           // Google calendar / contacts
    AccessibleStringListModel calendarlist;         // Contents of Calendar display
    QTimer *timer ;
    Encryption *enc ;
    AdvancedFind *find ;
    QList<QDateTime> calendarrowends ;              // The end of each row in the calendar list, so Goto Today can find the nearest
    QList<QDateTime> calendarrowstarts ;            // The start of each row in the calendar list, so Goto Date can find a date
    bool titledEntriesHeld ;                        // An email or phone entry was refused: the form is not reloaded until it is fixed

    // Only the copy of ContactManager holding the data folder's lock sends messages, so two copies cannot send the same
    // one.  The lock is held until this copy closes.
    QLockFile *datalock ;
    bool remindersallowed ;

    QString SMTPSMSErrorMessage ;

    // The log window, which is not modal: one is shown at a time, and it is cleared when it closes
    QPointer<LogForm> logwindow ;

    // Manage dialog controls / enables etc.
    void UpdateContactsList() ;          // Update drop-down list
    void populateDialog(QString id, bool switchtooverview=true) ;    // Update dialog with requested details

    void refreshMenus(Contact &contact, bool isediting, bool isnew, bool resetcursorpos) ;      // Enable menus for contact

    // Transfer data to/from dialog
    void LoadTabs() ;               // Loads / Populates all Tabs
    bool SaveTabs(bool explicitsave=false) ;   // Saves all Tabs.  Returns false when the contact details were refused.  File > Save checks every entry
    void closeEvent(QCloseEvent *event) override ;   // Keeps the window open while an entry is refused
    void logLoadSummary() ;         // Logs counts of what was loaded, for diagnosis

    void appendHistory(Contact& contact, QString msg) ;

    void LoadOverviewTab() ;  // Transfer Database Overview -> Overview Tab
    void LoadContactTab() ;     // Transfer Database Item Contact Data -> Contact Tab
    void setupTitledEntries() ;     // F2 in an email or phone entry changes its title
    void refreshTitledEntries() ;   // Names each email and phone entry by its title
    void changeTitle(QLineEdit *entry) ;
    bool eventFilter(QObject *watched, QEvent *event) override ;
    void tidyTitledEntries() ;                  // Puts right the email and phone entries that can be, when the form is loaded
    void tidyTitledEntry(QLineEdit *entry) ;    // Puts right one email or phone entry, when focus leaves it
    bool ValidateTitledEntries(Contact &dr, bool always=false) ;   // Checks the email and phone entries when the form has an edit, or always when saved with File > Save
    Contact& formContact() ;                    // The contact the form shows, even when the selection has moved on
    QString formContactId ;                     // The ID of the contact the form was loaded with
    bool FormHasEdits(Contact &dr) ;            // True when the form has an edit that is not already in the contact
    bool ConfirmSaveOrDiscard(const QString &action) ;   // Asks Save, Discard or Cancel when the form has edits.  False when cancelled or refused
    void FormToContact(Contact &newdr) ;        // Copies the contact details on the form into newdr
    bool SaveContactTab(bool explicitsave=false) ;     // Transfer Contact Tab -> Database Item Contact Data
    void LoadCalendarTab() ;  // Transfer Global Calendar to Calendar Tab
    void SaveCalendarTab() ;    // Save the Calendar if Needed
    void LoadEditHistory() ;
    void SaveEditHistory() ;
    void LoadTodo() ;
    void SaveTodo() ;

    // Create a blank contact entry
    void createContact() ;

    // Update window title
    void setAccessibleTextAndWindowTitle(QString name) ;

    void editAppointment(Appointment &editing, Appointment &reference, QString contactid = QString(""), bool createnew = false, QString occurrence = QString("")) ;
    void storeAppointmentEdit(Appointment &result, QString occurrence = QString("")) ;   // Saves an edit; a repeat changes its appointment

    void doBackup() ;
    void sendMailSMS() ;
    void checkSMSKeyExpiry() ;

    void beep() ;
    void msg(QString msg) ;

    // Advanced Search
    bool searchContactRecordSet(QString searchtext, Contact &contact, Contact::ContactRecord record) ;

    // EMail & SMS
    bool loadMessage(QString filename, QString& from, QString& title,QString& message, Appointment& appt, Contact& contact) ;

};

#endif // MAINWINDOW_H
