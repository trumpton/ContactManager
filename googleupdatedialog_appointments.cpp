//
// googleupdatedialog_appointments.cpp
//

#include <QDateTime>
#include "googleupdatedialog.h"
#include "ui_googleupdatedialog.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"
#include <QElapsedTimer>

// TIMING: prints how long each sync phase took.  Uses phasetimer, which is
// declared where the download starts.
#define PHASE_DONE(name) dbg(QString("TIMING phase done: %1 - %2 ms").arg(name).arg(phasetimer.restart()))

// True if the app has changed the appointment since the last successful sync.  Every local edit sets its Updated
// date, and a download sets it to Google's update time.  Dates are kept to the second, so the comparison includes
// the second of the sync date (see the contact version).  Before any sync has completed, every entry counts as changed.
static bool changedLocallySince(Appointment &local, const QDateTime &lastsync)
{
    if (!lastsync.isValid()) return true ;
    QDateTime updated = local.getDate(Appointment::Updated) ;
    return updated >= lastsync ;
}

bool GoogleUpdateDialog::processCalendarUpdate(QDateTime &lastsync, GoogleAccess& google, class ContactDatabase& db, class Calendar& cal)
{
    bool networkok = true ;
    int windowbefore = gConf->calendarWindowBefore() ;
    int windowafter = gConf->calendarWindowAfter() ;
    QDateTime fromwindow = QDateTime::currentDateTimeUtc().addDays(-1*windowbefore).addSecs(3600) ;
    QDateTime towindow = QDateTime::currentDateTimeUtc().addDays(windowafter).addSecs(-3600) ;
    Calendar googlecal ;

    // ------------------------------------------------------------------------------------
    //
    // 1. Download Google Calendar List (all, inc purged)
    //

    ui->updateStatusReport->append(QString("  Downloading New and Changed Appointments from Google:")) ;
    setProgress(0) ;
    QElapsedTimer phasetimer ;
    phasetimer.start() ;
    networkok &= google.getCalendar(googlecal, qMax(windowbefore, GOOGLE_APPOINTMENT_LOOKBACK_DAYS), windowafter) ;
    PHASE_DONE("download appointments") ;
    googlecal.dumpEntries("google", db) ;
    int googlesize = googlecal.size() ;
    for (int i=0; i<googlesize; i++) {
        Appointment &googleappt = googlecal.getAppointment(i) ;
        ui->updateStatusReport->append(QString("    ") + googleappt.asText()) ;
    }
    ui->updateStatusReport->append(QString("    Downloaded: ") + QString::number(googlesize) + QString(" entries")) ;

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 2. Set the default upload / download
    //
    setProgress(100) ;
    bool googlemaster = gConf->googleMaster() ;

    for (int i=0; i<cal.size(); i++) {

        Appointment& localappt = cal.getAppointment(i) ;

        bool upload ;

        if (googlemaster) {
            // Google Master, So Upload by default
            upload=false ;
        } else {
            // Local Master.  Upload only what the app has changed since the last sync.  Everything else is downloaded,
            // so a change made on Google is kept, and an unchanged entry is not written back to Google.
            upload = changedLocallySince(localappt, lastsync) ;
        }

        if (localappt.isTemp()) {
            localappt.setFlag(Appointment::ToBeDownloaded, false) ;
            localappt.setFlag(Appointment::ToBeUploaded, false) ;
        } else {
            // Read-only entries (from another Google calendar) are only ever downloaded
            bool uploadentry = upload && !localappt.isSet(Appointment::InternetOwned) ;
            localappt.setFlag(Appointment::ToBeDownloaded, !uploadentry) ;
            localappt.setFlag(Appointment::ToBeUploaded, uploadentry) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 3. Fix broken ID links
    //
    setProgress(150) ;
    ui->updateStatusReport->append(QString("  Repairing Broken Local Google Record IDs:")) ;
    for (int i=0, size=googlecal.size(); i<size; i++) {
        Appointment& googleappt = googlecal.getAppointment(i) ;
        Appointment& localappt = cal.getAppointmentBy(Appointment::ID, googleappt.getField(Appointment::ID)) ;
        if (!localappt.isNull() && !googleappt.isSet(Appointment::Deleted) &&
                localappt.getField(Appointment::GoogleRecordId).compare(googleappt.getField(Appointment::GoogleRecordId))!=0) {
            localappt.setField(Appointment::GoogleRecordId, googleappt.getField(Appointment::GoogleRecordId)) ;
            ui->updateStatusReport->append(QString("    ") + localappt.asText()) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 4. Add Fake entry in Google list for missing records, and identify new local records
    //
    setProgress(200) ;
    PHASE_DONE("before identifying local appointments") ;
    ui->updateStatusReport->append(QString("  Identifying local appointments missing on Google:")) ;

    for (int i=0, size=cal.size(); i<size; i++) {

        Appointment& localappt = cal.getAppointment(i) ;
        QDateTime from = localappt.getDate(Appointment::From) ;
        QDateTime to = localappt.getDate(Appointment::To) ;

        // Just process non-temp, non-deleted entries within sync window
        if ( !localappt.isTemp() && !localappt.isSet(Appointment::Deleted) &&
              ( (from <= fromwindow && to >= fromwindow) ||
                (from >= fromwindow && to <= towindow) ||
                ( from <= towindow && to >= towindow) ) ) {

            Appointment& googleappt = googlecal.getAppointmentBy(Appointment::ID, localappt.getField(Appointment::ID)) ;

            if (googleappt.isNull()) {

                // Add Dummy Entry to googlecalendar
                Appointment deletedappt ;
                deletedappt.setField(Appointment::ID, localappt.getField(Appointment::ID)) ;
                deletedappt.setFlag(Appointment::Deleted, true) ;
                googlecal.addAppointment(deletedappt) ;

                if (localappt.getField(Appointment::GoogleRecordId).isEmpty()) {

                    // Appointment has never been uploaded
                    // Override upload/download -> upload
                    localappt.setFlag(Appointment::ToBeUploaded, true) ;
                    localappt.setFlag(Appointment::ToBeDownloaded, false) ;
                    ui->updateStatusReport->append(QString("    %1 (new local)").arg(localappt.asText())) ;

                } else {

                    // Appointment has previously been uploaded
                    // So don't override options (stick with default)
                    // But, forget any previous Google Record ID
                    localappt.setField(Appointment::GoogleRecordId, "") ;
                    ui->updateStatusReport->append(QString("    %1 (removed google)").arg(localappt.asText())) ;

                }

            }

        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 5. Add Fake entry in Local list for missing records, and identify new google records
    //
    setProgress(250) ;
    PHASE_DONE("identifying local appointments") ;
    ui->updateStatusReport->append(QString("  Identifying Google appointments missing locally:")) ;
    for (int i=0, size=googlecal.size(); i<size; i++) {

        Appointment& googleappt = googlecal.getAppointment(i) ;
        Appointment& localappt = cal.getAppointmentBy(Appointment::ID, googleappt.getField(Appointment::ID)) ;

        // Google birthdays are not stored: they are shown from the contacts list.  Storing
        // them made a new copy on every sync, as their IDs are not kept by Google.
        if (localappt.isNull() && !googleappt.isSet(Appointment::Deleted) && !googleappt.isBirthday()) {
            Appointment newlocal ;
            newlocal.setField(Appointment::ID, googleappt.getField(Appointment::ID)) ;
            newlocal.setField(Appointment::From, googleappt.getField(Appointment::From)) ;
            newlocal.setField(Appointment::To, googleappt.getField(Appointment::To)) ;
            newlocal.setField(Appointment::Summary, QString("--! Undefined !--")) ;

            // Not confirmed again.  The appointment may have been messaged before its local record was lost, or was made
            // elsewhere, so its confirmation is not sent.  Changes made to it after this are still sent as updates.
            newlocal.setField(Appointment::MessageSent, nowToIsoString()) ;

            // Download only, and not uploaded.  An event made on Google has no ID stored there, but its local ID is
            // derived from its Google ID (see parseAppointment), so it is stable without an upload.  Uploading it
            // rewrote the event on Google, adding the app's properties, and rewrote events the user never changed.
            newlocal.setFlag(Appointment::ToBeDownloaded, true) ;
            newlocal.setFlag(Appointment::InternetOwned, googleappt.isSet(Appointment::InternetOwned)) ;
            cal.addAppointment(newlocal) ;
            ui->updateStatusReport->append(QString("        ") + googleappt.asText()) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 6. Synchronise changes (download first)
    //

    setProgress(300) ;
    PHASE_DONE("identifying google appointments") ;
    ui->updateStatusReport->append(QString("  Synchronising Changes:")) ;

    QElapsedTimer synctimer ;
    synctimer.start() ;
    int uploadcount=0 ;
    qint64 uploadms=0 ;
    for (int i=0, size=cal.size(); i<size; i++) {

        if (i % 50 == 0) {
            dbg(QString("TIMING sync appointments: %1/%2 items, %3 ms, %4 uploads so far (%5 ms uploading), log %6 chars")
                .arg(i).arg(size).arg(synctimer.elapsed()).arg(uploadcount).arg(uploadms).arg(getLog().size())) ;
        }

        setProgress(300 + ( i * 200)/size ) ;
        if (state<0) return false ;

        Appointment& localappt = cal.getAppointment(i) ;
        QDateTime from = localappt.getDate(Appointment::From) ;
        QDateTime to = localappt.getDate(Appointment::To) ;

        // Just process non-temp entries within sync window.  An entry that Google has, or that is to be downloaded, is
        // processed whatever its date: a repeat, or an entry older than the window, is downloaded with its details.
        bool ongoogle = !googlecal.getAppointmentBy(Appointment::ID, localappt.getField(Appointment::ID)).isNull() ;
        if ( !localappt.isTemp() &&
               ( ongoogle || localappt.isSet(Appointment::ToBeDownloaded) ||
                 (from <= fromwindow && to >= fromwindow) ||
                 (from >= fromwindow && to <= towindow) ||
                 ( from <= towindow && to >= towindow) )  ) {

            Appointment& googleappt = googlecal.getAppointmentBy(Appointment::ID, localappt.getField(Appointment::ID)) ;

            bool deletedlocaldeletedgoogle = (googleappt.isNull() || googleappt.isSet(Appointment::Deleted)) && (localappt.isSet(Appointment::Deleted)) ;
            bool googlematcheslocal = googleappt.matches(localappt, Appointment::maDetails|Appointment::maId) ;

            if ( deletedlocaldeletedgoogle ) {

                // Deleted everywhere, so nothing is required
                localappt.setFlag(Appointment::ToBeUploaded, false) ;
                localappt.setFlag(Appointment::ToBeDownloaded, false) ;

            } else if (!googlematcheslocal) {

                QString changes ;
                QString summary ;

                // Download Details from Google
                if (localappt.isSet(Appointment::ToBeDownloaded)) {
                    if (googleappt.isSet(Appointment::Deleted)) {
                        changes = QString("Deleted.") ;
                        summary = QString("deleted") ;
                        localappt.setFlag(Appointment::Deleted, true) ;
                    } else {
                        changes = QString("Downloaded: ") + googleappt.mismatch(localappt, Appointment::maDetails) + QString(". ");
                        summary = QString("downloaded") ;
                        googleappt.copyTo(localappt, Appointment::maDetails|Appointment::maGoogleAcct) ;
                    }
                    ui->updateStatusReport->append(QString("    ") + localappt.asText() + QString(" - Updating Local Appointment (%1)").arg(summary)) ;
                }

                // Update Details on Google
                if (localappt.isSet(Appointment::ToBeUploaded)) {
                    if (localappt.isSet(Appointment::Deleted)) {
                        changes = QString("Deleted.") ;
                        summary = QString("deleted") ;
                    } else {
                        changes = QString("Uploaded: ") + localappt.mismatch(googleappt, Appointment::maDetails|Appointment::maId) + QString(". ");
                        summary = QString("uploaded") ;
                    }
                    ui->updateStatusReport->append(QString("    ") + localappt.asText() + QString(" - Updating Google Appointment (%1)").arg(summary)) ;

                    bool status ;
                    if (localappt.getField(Appointment::GoogleRecordId).isEmpty()) {
                        QElapsedTimer uploadtimer ;
                        uploadtimer.start() ;
                        status = google.updateAppointment(localappt, GoogleAccess::Post) ;
                        uploadcount++ ;
                        uploadms += uploadtimer.elapsed() ;
                        dbg(QString("TIMING appointment upload #%1 POST %2 ms ok=%3").arg(uploadcount).arg(uploadtimer.elapsed()).arg(status)) ;
                    } else {
                        QElapsedTimer uploadtimer ;
                        uploadtimer.start() ;
                        status = google.updateAppointment(localappt, GoogleAccess::Put) ;
                        uploadcount++ ;
                        uploadms += uploadtimer.elapsed() ;
                        dbg(QString("TIMING appointment upload #%1 PUT %2 ms ok=%3").arg(uploadcount).arg(uploadtimer.elapsed()).arg(status)) ;
                    }
                    networkok &= status ;

                    if (status) {
                        addLog(QString("    ") + localappt.asText() + QString(" - Uploading Google Appointment Details: %1").arg(changes)) ;
                        localappt.copyTo(googleappt, Appointment::maDetails|Appointment::maId|Appointment::maGoogleId) ;
                    } else {
                        addLog(QString("    ") + localappt.asText() + QString(" - FAILED Uploading Google Appointment Details: %1").arg(changes)) ;
                        networkok = false ;
                    }

                    // If the record has been successfully removed from Google, forget the local details
                    if (networkok && localappt.isSet(Appointment::Deleted)) {
                        localappt.setField(Appointment::GoogleRecordId, QString("")) ;
                        localappt.setFlag(Appointment::ToBeUploaded, false) ;
                        localappt.setFlag(Appointment::ToBeDownloaded, false) ;
                    }

                }

            } // if in sync window

        } // for each calendar entry

    }

    if (state<0) return false ;

    PHASE_DONE("synchronising appointments") ;
    dbg(QString("TIMING sync appointments done: %1 uploads, %2 ms uploading, %3 ms in sync loop")
        .arg(uploadcount).arg(uploadms).arg(synctimer.elapsed())) ;
    if (networkok) {
        ui->updateStatusReport->append(QString("  Appointment Synchronisation: OK")) ;
    } else {
        ui->updateStatusReport->append(QString("  Appointment Synchronisation: FAILED - Please resync")) ;
    }

    setProgress(500) ;
    return networkok ;
}
