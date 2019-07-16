#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include <QTimer>
#include "contactdatabase.h"
#include "calendar.h"
#include "configuration.h"
#include "googleaccess.h"
#include "accessiblestringlistmodel.h"

#include "ui_mainwindow.h"
#include "search.h"
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

#include "smtp.h"
#include "sms.h"

#include <QFileDialog>
#include <QTextStream>
#include <QInputDialog>
#include <QTextCodec>



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

    void on_actionNewMyAppointment_triggered();
    void on_actionNewAppointment_triggered();
    void on_actionInsert_My_Appointment_After_triggered();
    void on_action_Insert_Appointment_After_triggered();
    void on_actionMoveAppointment_triggered();
    void on_actionRemoveAppointment_triggered();

    void on_newAppointmentButton_clicked();
    void on_insertAppointmentButton_clicked();
    void on_moveAppointmentButton_clicked();
    void on_removeAppointmentButton_clicked();

    void on_actionEnableContactDetailsEdit_triggered();
    void on_actionDeleteContact_triggered();
    void on_actionAbout_triggered();
    void on_actionFind_triggered();
    void on_actionFindNext_triggered();
    void on_action_Google_Sync_triggered();
    void on_actionPayment_triggered();
    void on_actionIncome_triggered() ;
    void on_actionTransactionReport_triggered() ;
    void on_actionManual_triggered();
    void on_listCalendar_activated(const QModelIndex &index);
    void on_actionExportContacts_triggered();
    void on_actionExportAppointments_triggered();
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

    QString SMTPSMSErrorMessage ;

    // Manage dialog controls / enables etc.
    void UpdateContactsList() ;          // Update drop-down list
    void populateDialog(QString id, bool switchtooverview=true) ;    // Update dialog with requested details

    void refreshMenus(Contact &contact, bool isediting, bool isnew, bool resetcursorpos) ;      // Enable menus for contact

    // Transfer data to/from dialog
    void LoadTabs() ;               // Loads / Populates all Tabs
    void SaveTabs() ;               // Saves all Tabs

    void appendHistory(Contact& contact, QString msg) ;

    void LoadOverviewTab() ;  // Transfer Database Overview -> Overview Tab
    void LoadContactTab() ;     // Transfer Database Item Contact Data -> Contact Tab
    bool SaveContactTab() ;     // Transfer Contact Tab -> Database Item Contact Data
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

    void editAppointment(Appointment &editing, Appointment &reference, QString contactid = QString(""), bool createnew = false) ;

    void doBackup() ;
    void sendMailSMS() ;

    void beep() ;
    void msg(QString msg) ;

    // Advanced Search
    bool searchContactRecordSet(QString searchtext, Contact &contact, Contact::ContactRecord record) ;

    // EMail & SMS
    int SendMail(QString from, QString emailaddress, QString subject, QString message) ;
    int SendSMS(QString from, QString mobile, QString message) ;
    int GetSMSBalance() ;
    bool loadMessage(QString filename, QString& from, QString& title,QString& message, Appointment& appt, Contact& contact) ;

};

#endif // MAINWINDOW_H
