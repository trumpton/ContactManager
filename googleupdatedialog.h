#ifndef GOOGLEUPDATEDIALOG_H
#define GOOGLEUPDATEDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include "googleaccess.h"
#include "contactdatabase.h"

namespace Ui {
class GoogleUpdateDialog;
}

class GoogleUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoogleUpdateDialog(QWidget *parent = 0);
    ~GoogleUpdateDialog();
    int doUpdate(GoogleAccess &confgoogle, class ContactDatabase &confdb, class Calendar& cal) ;

    // Progress for other long operations (such as merging duplicates): the bar, and a log of what was done
    void showProgress(QString title) ;
    void reportProgress(int percent, QString line) ;
    bool isAborted() ;
    void finishProgress(QString summary) ;


private slots:
    void on_pushButton_clicked();

private:
    Ui::GoogleUpdateDialog *ui;
    int state ;

    // Sets the progress bar, and lets the window process events so that it keeps responding
    void setProgress(int value) ;

    // Sync the Contact Group (upload to google, "download" from google)
    bool updateSingleGoogleContactGroup(GoogleAccess &google, Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb) ;
    bool updateSingleLocalContactGroup(GoogleAccess &google, Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb) ;

    // Update the contacts
    bool processContactUpdate(QDateTime& lastsyncdate, GoogleAccess& google, ContactDatabase &db) ;

    // Upload contact to Google
    bool uploadContact(Contact& contact, GoogleAccess& google, QString etag = QString("")) ;

    // Update the calendar
    bool processCalendarUpdate(QDateTime &lastsyncdate, GoogleAccess& google, class ContactDatabase& db, class Calendar& cal) ;

    // Support functions or the processCalendarUpdate
    bool calendarFetchChangedGoogleRecords(QProgressBar *bar, GoogleAccess& google, QDateTime &lastsync, Calendar& googlecalendar) ;
    bool calendarFetchGoogleRecordsForChangedLocalRecords(QProgressBar *bar, GoogleAccess &google, QDateTime &lastsync, Calendar &googlecalendar, Calendar &cal) ;
    bool calendarSaveNewGoogleEntries(QProgressBar *bar, GoogleAccess &google, QDateTime &lastsync, Calendar &googlecalendar, Calendar &cal) ;
    bool calendarUploadNewEntriesToGoogle(QProgressBar *bar, GoogleAccess &google, QDateTime &lastsync, Calendar &googlecalendar, Calendar &cal) ;
    bool calendarProcessChanges(QProgressBar *bar, GoogleAccess &google, QDateTime &lastsync, Calendar &googlecalendar, Calendar &cal) ;
    bool appointmentUploadChangesToGoogle(GoogleAccess &google, Appointment &googleappt, Appointment &appt) ;
    bool appointmentDownloadChangesFromGoogle(Appointment &googleappt, Appointment &appt) ;

};

#endif // GOOGLEUPDATEDIALOG_H
