#ifndef GOOGLEUPDATEDIALOG_H
#define GOOGLEUPDATEDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QSet>
#include "googleaccess.h"
#include "contactdatabase.h"

namespace Ui {
class GoogleUpdateDialog;
}

// Uploads are saved as they happen, and a checkpoint is reported every SYNC_UPLOAD_CHUNK uploads.  A sync that stops
// part-way keeps what it has uploaded, and the next sync does not upload those items again.
#define SYNC_UPLOAD_CHUNK 100

// The uploads of one phase ("calendar" or "contacts"), kept in a file, so that a sync that stops part-way does not repeat
// them.  The file belongs to one last-sync date, and is removed when the phase completes.
QSet<QString> loadUploadProgress(const QString &phase, const QString &lastsync) ;
void recordUploadProgress(const QString &phase, const QString &lastsync, const QString &key) ;
void clearUploadProgress(const QString &phase) ;
QString uploadProgressKey(const QString &id, const QString &updated) ;

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

    // Each phase reports its progress from 0 to 1000 in its own terms.  The contacts take the first half of the bar, and the
    // calendar the second half, so this is added to what a phase reports.
    int progressoffset ;

    // Sets the progress bar, and lets the window process events so that it keeps responding
    void setProgress(int value) ;

    // Sync the Contact Group (upload to google, "download" from google)
    bool updateSingleGoogleContactGroup(GoogleAccess &google, Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb) ;
    bool updateSingleLocalContactGroup(GoogleAccess &google, Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb) ;

    // Update the contacts
    bool processContactUpdate(QDateTime& lastsyncdate, GoogleAccess& google, ContactDatabase &db) ;

    // Once the contacts are synchronised, each appointment's linked contact is checked against them (issue #95)
    void settleAppointmentContacts(Calendar &cal, ContactDatabase &db, int &kept, int &named, int &unnamed) ;

    // Upload contact to Google
    bool uploadContact(Contact& contact, GoogleAccess& google, Contact& googlecontact) ;

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
