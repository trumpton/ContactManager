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

// True when an entry in the window has a Google record but no snapshot of what Google held (issue #105).  Under Google master
// such an entry cannot be compared with Google by an incremental listing, so the listing is done in full once.
static bool anyEntryWithoutGoogleSnapshot(Calendar &cal, const QDateTime &fromwindow, const QDateTime &towindow)
{
    for (int i=0; i<cal.size(); i++) {
        Appointment &appt = cal.getAppointment(i) ;
        if (appt.isTemp() || appt.isSet(Appointment::Deleted) || appt.getField(Appointment::GoogleRecordId).isEmpty()) continue ;
        QDateTime from = appt.getDate(Appointment::From) ;
        QDateTime to = appt.getDate(Appointment::To) ;
        bool inwindow = (from <= fromwindow && to >= fromwindow) || (from >= fromwindow && to <= towindow) || (from <= towindow && to >= towindow) ;
        if (inwindow && !appt.hasGoogleSnapshot()) return true ;
    }
    return false ;
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
    // The changes since the last listing, when the sync token was given for these windows (issue #96).  Otherwise everything is listed.
    QString tokenkey = QString("%1,%2").arg(windowbefore).arg(windowafter) ;
    QString stored = gConf->getLastGoogleCalendarSyncToken() ;
    QString syncToken = stored.startsWith(tokenkey + "|") ? stored.mid(tokenkey.size() + 1) : QString() ;
    bool incremental = !syncToken.isEmpty() ;
    if (incremental && gConf->googleMaster() && anyEntryWithoutGoogleSnapshot(cal, fromwindow, towindow)) {
        // Under Google master the entries are compared with their snapshots, so the listing is done in full, once, to give each
        // entry in the window its snapshot (issue #105)
        ui->updateStatusReport->append(QString("  Some appointments have no record of what Google holds, so every appointment is listed.")) ;
        incremental = false ;
        syncToken.clear() ;
    }
    QStringList deletedids ;
    networkok &= google.getCalendar(googlecal, &deletedids, &syncToken) ;
    // Google refuses a sync token it no longer holds (410), or one it does not recognise (400): the listing is then done again in full
    if (!networkok && incremental && (google.getNetworkErrorCode()==410 || google.getNetworkErrorCode()==400)) {
        // Google no longer accepts the sync token, so everything is listed again
        ui->updateStatusReport->append(QString("  Google did not accept the sync token (HTTP %1), so every appointment is listed again.").arg(google.getNetworkErrorCode())) ;
        networkok = true ;
        incremental = false ;
        syncToken.clear() ;
        googlecal.clear() ;
        deletedids.clear() ;
        networkok &= google.getCalendar(googlecal, &deletedids, &syncToken) ;
    }
    ui->updateStatusReport->append(incremental ? QString("  Listing: the changes since the last sync.") : QString("  Listing: every appointment.")) ;
    PHASE_DONE("download appointments") ;
    googlecal.dumpEntries("google", db) ;
    int googlesize = googlecal.size() ;
    for (int i=0; i<googlesize; i++) {
        Appointment &googleappt = googlecal.getAppointment(i) ;
        ui->updateStatusReport->append(QString("    ") + googleappt.asText()) ;
    }
    ui->updateStatusReport->append(QString("    Downloaded: ") + QString::number(googlesize) + QString(" entries")) ;

    // Archived appointments are checked against Google first (issue #96): one Google has cancelled is removed from the archive,
    // one now in the window is restored, and one still outside it is kept up to date.  The IDs still archived are not downloaded
    // as new local appointments.
    QSet<QString> archivedids ;
    int restored=0, archivedupdated=0, archivedremoved=0 ;
    reconcileArchive(cal, googlecal, deletedids, fromwindow, towindow, archivedids, restored, archivedupdated, archivedremoved) ;
    ui->updateStatusReport->append(QString("  Archive: %1 restored, %2 updated, %3 removed as cancelled on Google.")
                                   .arg(restored).arg(archivedupdated).arg(archivedremoved)) ;

    // Repeats that have passed the lower edge of the window start at their first occurrence still in it (issue #96).  The start
    // changes, so the change is uploaded with this sync.
    int trimmed=0 ;
    for (int i=0; i<cal.size(); i++) {
        Appointment &appt = cal.getAppointment(i) ;
        if (appt.isTemp() || appt.isSet(Appointment::Deleted) || !appt.isRepeating()) continue ;
        if (appt.trimRepeatBefore(fromwindow)) trimmed++ ;
    }
    if (trimmed>0) ui->updateStatusReport->append(QString("  Repeats starting in the window: %1 moved on to their first occurrence in it.").arg(trimmed)) ;

    // A repeat with no occurrence left is removed.  When ContactManager is master it is marked Deleted, so the upload below
    // deletes it on Google too.  With Google as master, the change is Google's to make, so nothing is changed here.
    int norepeats=0 ;
    if (!gConf->googleMaster()) {
        for (int i=0; i<cal.size(); i++) {
            Appointment &appt = cal.getAppointment(i) ;
            if (appt.isTemp() || appt.isSet(Appointment::Deleted) || !appt.isRepeating()) continue ;

            // Only a repeat Google has is deleted there.  One Google never had is left hidden, so it is not created on Google
            if (appt.getField(Appointment::GoogleRecordId).isEmpty()) continue ;
            if (appt.hasNoOccurrences()) {
                appt.setFlag(Appointment::Deleted, true) ;
                norepeats++ ;
            }
        }
    }
    if (norepeats>0) ui->updateStatusReport->append(QString("  Repeats with no occurrence left: %1 removed.").arg(norepeats)) ;

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
            // A sent state the Google event could not take is uploaded now (see LedgerPending)
            upload = changedLocallySince(localappt, lastsync) || localappt.getField(Appointment::LedgerPending).compare("yes")==0 ;
        }

        if (localappt.isTemp()) {
            localappt.setFlag(Appointment::ToBeDownloaded, false) ;
            localappt.setFlag(Appointment::ToBeUploaded, false) ;
        } else {
            // Read-only entries (from another Google calendar) are only ever downloaded
            // An entry with no real start is never uploaded, as it would make a blank event on Google (issue #104)
            bool hasstart = localappt.hasStart() ;
            bool uploadentry = upload && hasstart && !localappt.isSet(Appointment::InternetOwned) ;
            localappt.setFlag(Appointment::ToBeDownloaded, !uploadentry) ;
            localappt.setFlag(Appointment::ToBeUploaded, uploadentry) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 3. Fix broken ID links
    //
    // An appointment that has not changed is not in an incremental listing.  Its Google copy is taken to be its local copy, so that
    // nothing is downloaded for it.  A change made locally is still uploaded: see the upload test in step 6.
    // The ids whose Google copy is taken to be the local entry, because there is no snapshot to take it from.  Their snapshots
    // are not refreshed from that copy.
    QSet<QString> fromlocal ;
    if (incremental) {
        for (int i=0; i<cal.size(); i++) {
            Appointment &localappt = cal.getAppointment(i) ;
            QString id = localappt.getField(Appointment::ID) ;
            if (localappt.isTemp() || localappt.isSet(Appointment::Deleted) || localappt.getField(Appointment::GoogleRecordId).isEmpty()) continue ;
            if (deletedids.contains(id)) continue ;
            QDateTime from = localappt.getDate(Appointment::From) ;
            QDateTime to = localappt.getDate(Appointment::To) ;
            bool inwindow = (from <= fromwindow && to >= fromwindow) || (from >= fromwindow && to <= towindow) || (from <= towindow && to >= towindow) ;
            if (!inwindow || !googlecal.getAppointmentBy(Appointment::ID, id).isNull()) continue ;
            // Under Google master, the Google copy is what Google held at the last sync, so a local edit is overwritten by it
            Appointment copy = localappt ;
            if (!(googlemaster && localappt.googleSnapshotCopy(copy))) {
                copy = localappt ;
                fromlocal.insert(id) ;
            }
            googlecal.addAppointment(copy, false) ;
        }
    }

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
        // An appointment that has expired is not created here: it is only on Google (issue #96)
        if (localappt.isNull() && !googleappt.isSet(Appointment::Deleted) && !googleappt.isBirthday()
                && !archivedids.contains(googleappt.getField(Appointment::ID)) && !googleappt.isExpiredBefore(fromwindow)) {
            Appointment newlocal ;
            newlocal.setField(Appointment::ID, googleappt.getField(Appointment::ID)) ;
            newlocal.setField(Appointment::From, googleappt.getField(Appointment::From)) ;
            newlocal.setField(Appointment::To, googleappt.getField(Appointment::To)) ;
            newlocal.setField(Appointment::Summary, QString("--! Undefined !--")) ;

            // The sent state kept on Google is taken in.  An appointment with none is not confirmed, as it may have been
            // messaged before its local record was lost, or was made elsewhere.  Changes made to it are still sent.
            googleappt.copyTo(newlocal, Appointment::maSentState) ;
            if (newlocal.getField(Appointment::MessageSent).isEmpty()) {
                newlocal.setField(Appointment::MessageSent, nowToIsoString()) ;
            }

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

    // Uploads done by an earlier sync that stopped part-way are not repeated
    QString lastsyncstr = lastsync.toString(Qt::ISODate) ;
    QSet<QString> doneuploads = loadUploadProgress("calendar", lastsyncstr) ;
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

            // The sent state kept on Google is taken in first, so a message sent from another copy is not sent again here
            if (!googleappt.isNull() && !googleappt.isSet(Appointment::Deleted)) localappt.mergeSentState(googleappt) ;

            bool deletedlocaldeletedgoogle = (googleappt.isNull() || googleappt.isSet(Appointment::Deleted)) && (localappt.isSet(Appointment::Deleted)) ;
            bool googlematcheslocal = googleappt.matches(localappt, Appointment::maDetails|Appointment::maId) ;
            if (googlematcheslocal && !googleappt.isNull() && !googleappt.isSet(Appointment::Deleted) &&
                    !fromlocal.contains(localappt.getField(Appointment::ID))) {
                localappt.setGoogleSnapshot(googleappt) ;
            }

            if ( deletedlocaldeletedgoogle ) {

                // Deleted everywhere, so nothing is required
                localappt.setFlag(Appointment::ToBeUploaded, false) ;
                localappt.setFlag(Appointment::ToBeDownloaded, false) ;

            } else if (!googlematcheslocal || localappt.isSet(Appointment::ToBeUploaded)) {

                QString changes ;
                QString summary ;

                // Download Details from Google
                // A local entry with nothing on Google, such as one outside the window, has nothing to download, so it is kept as
                // it is.  Copying the empty entry over it blanked it (issue #104).
                bool nogoogle = googleappt.isNull() || (!googleappt.isSet(Appointment::Deleted) && !googleappt.hasStart()) ;
                if (localappt.isSet(Appointment::ToBeDownloaded) && nogoogle) {
                    localappt.setFlag(Appointment::ToBeDownloaded, false) ;
                } else if (localappt.isSet(Appointment::ToBeDownloaded)) {
                    if (googleappt.isSet(Appointment::Deleted)) {
                        changes = QString("Deleted.") ;
                        summary = QString("deleted") ;
                        localappt.setFlag(Appointment::Deleted, true) ;
                    } else {
                        changes = QString("Downloaded: ") + googleappt.mismatch(localappt, Appointment::maDetails) + QString(". ");
                        summary = QString("downloaded") ;
                        googleappt.copyTo(localappt, Appointment::maDetails|Appointment::maGoogleAcct) ;
                        if (!fromlocal.contains(localappt.getField(Appointment::ID))) localappt.setGoogleSnapshot(googleappt) ;
                    }
                    ui->updateStatusReport->append(QString("    ") + localappt.asText() + QString(" - Updating Local Appointment (%1)").arg(summary)) ;
                }

                // Update Details on Google.  An upload that an earlier sync made is not repeated.
                QString uploadkey = uploadProgressKey(localappt.getField(Appointment::ID), localappt.getField(Appointment::Updated)) ;
                if (doneuploads.contains(uploadkey)) localappt.setFlag(Appointment::ToBeUploaded, false) ;
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
                    bool hadgoogleid = !localappt.getField(Appointment::GoogleRecordId).isEmpty() ;
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

                    // A Google event that has been created is linked straight away, even if its reply did not match, so that
                    // a later sync updates it rather than creating it again
                    if (!hadgoogleid && !localappt.getField(Appointment::GoogleRecordId).isEmpty()) localappt.save(gCalendarSavePath) ;

                    if (status) {
                        addLog(QString("    ") + localappt.asText() + QString(" - Uploading Google Appointment Details: %1").arg(changes)) ;
                        localappt.copyTo(googleappt, Appointment::maDetails|Appointment::maId|Appointment::maGoogleId) ;
                        localappt.setGoogleSnapshot(localappt) ;

                        // Saved as it is uploaded, so that a Google event that was created is linked, and not created again
                        // if the sync stops.  The checkpoint below saves the calendar every SYNC_UPLOAD_CHUNK uploads.
                        localappt.save(gCalendarSavePath) ;
                        recordUploadProgress("calendar", lastsyncstr, uploadkey) ;
                        if (uploadcount % SYNC_UPLOAD_CHUNK == 0) {
                            if (!cal.save()) {
                                ui->updateStatusReport->append(QString("  Could not save the calendar at upload %1, so the sync stops here.").arg(uploadcount)) ;
                                return false ;
                            }
                            ui->updateStatusReport->append(QString("  Checkpoint: %1 uploads saved.").arg(uploadcount)) ;
                        }
                    } else {
                        // The reason is the network error if there was one, otherwise the last log entry from the upload, which says
                        // what did not match.  It is read before this failure is logged.
                        QStringList recent = QString(getLog()).split("\n", Qt::SkipEmptyParts) ;
                        QString reason = google.getNetworkError().isEmpty() ? (recent.isEmpty() ? QString() : recent.last().trimmed()) : google.getNetworkError() ;
                        ui->updateStatusReport->append(QString("    ") + localappt.asText() + QString(" - FAILED Uploading Google Appointment: ") + reason) ;
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

    // Expired appointments move to the archive, once they are uploaded and not changed (issue #96).  Saved, so that the moves and
    // the repeats that were moved on are kept.
    QStringList archivednow ;
    int archivedcount = cal.archiveExpired(fromwindow, lastsync, archivednow) ;
    if (!cal.save()) ui->updateStatusReport->append(QString("  The calendar could not be saved after archiving."))  ;
    ui->updateStatusReport->append(QString("  Archived %1 expired appointments.").arg(archivedcount)) ;

    PHASE_DONE("synchronising appointments") ;
    dbg(QString("TIMING sync appointments done: %1 uploads, %2 ms uploading, %3 ms in sync loop")
        .arg(uploadcount).arg(uploadms).arg(synctimer.elapsed())) ;
    if (networkok) {
        clearUploadProgress("calendar") ;

        // The token for the next listing is kept only when the phase completes, so that a sync which stops part-way lists these
        // changes again (issue #96)
        if (!syncToken.isEmpty()) gConf->setLastGoogleCalendarSyncToken(tokenkey + "|" + syncToken) ;
        ui->updateStatusReport->append(QString("  Appointment Synchronisation: OK")) ;
    } else {
        ui->updateStatusReport->append(QString("  Appointment Synchronisation: FAILED - Please resync")) ;
    }

    setProgress(500) ;
    return networkok ;
}

//=====================================================================================================
//
// Appointments and their contacts (issue #95).  Run only after the contacts have synchronised, as the contact list is then
// complete.  A link to a contact that exists is kept, and a blank For name is filled from the contact.  A link to no contact
// is matched by name, when exactly one contact has that name; otherwise the link is removed and For is set to "Unnamed Contact".
// An appointment with no linked contact, or linked to Me, is not changed.
//

void GoogleUpdateDialog::settleAppointmentContacts(Calendar &cal, ContactDatabase &db, int &kept, int &named, int &unnamed, int &nocontact)
{
    QSet<QString> ids ;
    QHash<QString, QStringList> idsbyname ;
    QHash<QString, QString> namebyid ;
    for (int i=0; i<db.size(); i++) {
        Contact &c = db.getContact(i) ;
        QString id = c.getField(Contact::ID) ;
        if (id.isEmpty()) continue ;
        QString name = c.getFormattedName(false, false).trimmed() ;
        ids.insert(id) ;
        namebyid.insert(id, name) ;
        if (!name.isEmpty()) idsbyname[name.toLower()] << id ;
    }

    QString me = gConf->getMe() ;
    kept=0 ;
    named=0 ;
    unnamed=0 ;
    nocontact=0 ;
    for (int i=0; i<cal.size(); i++) {
        Appointment &appt = cal.getAppointment(i) ;
        if (appt.isTemp()) continue ;
        QString contactid = appt.getField(Appointment::ContactId) ;

        // No contact at all (the Me entries have Me as their contact, so they are not here): For is "Unnamed Contact", as
        // Google's reply gives it when there is no name stored
        if (contactid.isEmpty()) {
            if (appt.getField(Appointment::For).isEmpty()) {
                nocontact++ ;
                appt.setField(Appointment::For, QString(APPOINTMENT_UNNAMED_CONTACT)) ;
            }
            continue ;
        }
        if (contactid.compare(me)==0) continue ;

        QString resolved ;
        AppointmentContactRule rule = matchAppointmentContact(contactid, appt.getField(Appointment::For), ids, idsbyname, resolved) ;
        switch (rule) {
        case ContactKept:
            kept++ ;
            if (appt.getField(Appointment::For).isEmpty()) appt.setField(Appointment::For, namebyid.value(resolved)) ;
            break ;
        case ContactNamed:
            named++ ;
            appt.setField(Appointment::ContactId, resolved) ;
            break ;
        case ContactUnnamed:
            unnamed++ ;
            appt.setField(Appointment::ContactId, QString()) ;
            appt.setField(Appointment::For, QString(APPOINTMENT_UNNAMED_CONTACT)) ;
            break ;
        }
    }
}

//=====================================================================================================
//
// Archived appointments, checked against Google (issue #96).  An archived appointment that Google has cancelled is removed from
// the archive.  One that Google holds inside the window is restored to the calendar, with Google's details and its Updated time
// kept, so that it is not uploaded again.  One that Google holds outside the window has its archived copy brought up to date.
// Every appointment left archived is in archivedids, so that the download does not make a new local copy of it.
//

void GoogleUpdateDialog::reconcileArchive(Calendar &cal, Calendar &googlecal, const QStringList &deletedids, QDateTime fromwindow, QDateTime towindow,
                                          QSet<QString> &archivedids, int &restored, int &updated, int &removed)
{
    restored=0 ;
    updated=0 ;
    removed=0 ;

    QList<Appointment> archive = Calendar::loadArchive() ;
    for (Appointment &archived : archive) {
        QString id = archived.getField(Appointment::ID) ;

        if (deletedids.contains(id)) {
            if (cal.removeFromArchive(id)) removed++ ;
            continue ;
        }

        // Not in this listing: left as it is, and not downloaded
        // An entry with no start is not used: copying it would blank the archived copy (issue #104)
        Appointment &googleappt = googlecal.getAppointmentBy(Appointment::ID, id) ;
        if (googleappt.isNull() || googleappt.isSet(Appointment::Deleted) || !googleappt.hasStart()) {
            archivedids.insert(id) ;
            continue ;
        }

        archived.copyFromKeepingUpdated(googleappt, Appointment::maDetails|Appointment::maGoogleAcct) ;

        QDateTime from = googleappt.getDate(Appointment::From) ;
        QDateTime to = googleappt.getDate(Appointment::To) ;
        bool inwindow = (from <= towindow && to >= fromwindow) ;
        if (inwindow) {
            if (cal.restoreFromArchive(archived)) restored++ ;
        } else {
            if (cal.updateArchived(archived)) updated++ ;
            archivedids.insert(id) ;
        }
    }
}

