
//
// googleupdatedialog_contacts.cpp
//


#include "googleupdatedialog.h"
#include "ui_googleupdatedialog.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"
#include <QElapsedTimer>

// TIMING: prints how long each sync phase took.  Uses phasetimer, which is
// declared where the download starts.
#define PHASE_DONE(name) dbg(QString("TIMING phase done: %1 - %2 ms").arg(name).arg(phasetimer.restart()))

//
// Process Contact Update
//
// 1. Fetch IDs
// 2. Download Google Contact List (all)
// 3. Add Fake entry in Google list for missing local contacts
// 4. Fix broken ID links
// 5. Store new non-deleted contacts and re-upload
// 6. Handle merges
// 7. Upload new local entries
// 8. Update profile
// 9. Synchronise changes
//


// The person field groups the upload writes, with the contact fields each one holds.  Only the groups
// where the app's value differs from Google's are written, so that Google keeps every other group as it
// is, including fields the app does not hold, such as a second URL.  userDefined is always written, as
// it holds the app's ID and upload marker.
static QString changedPersonFields(Contact &local, Contact &google)
{
    struct PersonGroup {
        const char *name ;
        QList<Contact::ContactRecord> fields ;
    } ;
    const QList<PersonGroup> groups = {
        { "names",          { Contact::Surname, Contact::Names } },
        { "emailAddresses", { Contact::Email, Contact::Email2, Contact::Email3, Contact::Email4, Contact::Email3Title, Contact::Email4Title } },
        { "phoneNumbers",   { Contact::Phone, Contact::Work, Contact::Mobile, Contact::Phone2, Contact::Phone3, Contact::Phone4,
                              Contact::Phone2Title, Contact::Phone3Title, Contact::Phone4Title } },
        { "sipAddresses",   { Contact::Voip } },
        { "organizations",  { Contact::Organisation } },
        { "addresses",      { Contact::Address, Contact::Address2 } },
        { "urls",           { Contact::Webaddress } },
        { "birthdays",      { Contact::Birthday } },
        { "biographies",    { Contact::Comments } },
    } ;

    QStringList changed ;
    for (const PersonGroup &group : groups) {
        for (Contact::ContactRecord field : group.fields) {
            if (local.getField(field) != google.getField(field)) {
                changed << group.name ;
                break ;
            }
        }
    }
    changed << "userDefined" ;
    return changed.join(",") ;
}

bool GoogleUpdateDialog::uploadContact(Contact& contact, GoogleAccess &google, Contact& googlecontact)
{
    bool success=false ;

    QString& googleid = contact.getField(Contact::GoogleRecordId) ;
    QString etag = googlecontact.getField(Contact::GoogleEtag) ;

    if (googleid.isEmpty() || etag.isEmpty()) {
        success = google.updateContact(contact, GoogleAccess::Post) ;
    } else {
        success = google.updateContact(contact, GoogleAccess::Patch, etag, changedPersonFields(contact, googlecontact)) ;
    }

    // Update Groups
    // Get and Validate Contact
    // Loop until OK
    // Store etag on success

    return success ;
}

bool GoogleUpdateDialog::processContactUpdate(QDateTime &lastsync, GoogleAccess& google, class ContactDatabase& db)
{
    Q_UNUSED(lastsync) ;

    bool networkok = true ;

    ContactDatabase googledb ;

    // ------------------------------------------------------------------------------------
    //
    // 1. Fetch IDs
    //
    setProgress(500) ;
    ui->updateStatusReport->append(QString("  Fetching Group IDs:")) ;
    if (!google.getGroupIds()) {
        ui->updateStatusReport->append(QString("    Creating Missing Groups.")) ;
        google.createGroupIds() ;
        google.getGroupIds() ;
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 2. Download Google Contact List (all)
    //
    setProgress(550) ;
    QElapsedTimer phasetimer ;
    phasetimer.start() ;
    networkok &= google.getContacts(googledb) ;
    PHASE_DONE("download contacts") ;
    ui->updateStatusReport->append(QString("  Downloaded ") + QString::number(googledb.size()) + QString(" Contacts from Google.")) ;

    // ------------------------------------------------------------------------------------
    //
    // 2b. Blank contacts that are on Google.  If this side has priority (local master, or
    //     updated last), Google's copy is removed too.  Otherwise Google's details are
    //     downloaded in the steps below, so nothing is lost.
    //
    int blankremoved=0 ;
    for (int i=0; i<db.size(); i++) {
        Contact& localcontact = db.getContact(i) ;
        if (localcontact.isSet(Contact::Deleted)) continue ;
        if (localcontact.getField(Contact::GoogleRecordId).isEmpty() || !localcontact.isBlank()) continue ;

        Contact& googlecontact = googledb.getContactById(localcontact.getField(Contact::ID)) ;

        // Copy the dates first: getDate() returns a shared object
        QDateTime localupdated = localcontact.getDate(Contact::Updated) ;
        QDateTime googleupdated = googlecontact.getDate(Contact::Updated) ;
        bool localpriority = !gConf->googleMaster() || (localupdated > googleupdated) ;

        if (googlecontact.isNull() || googlecontact.isBlank() || localpriority) {
            localcontact.setFlag(Contact::Deleted, true) ;
            blankremoved++ ;
        }
    }
    if (blankremoved>0) {
        ui->updateStatusReport->append(QString("  Removing %1 empty contact(s) from Google.").arg(blankremoved)) ;
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 3. Set the default upload / download
    //
    setProgress(600) ;
    bool googlemaster = gConf->googleMaster() ;

    for (int i=0; i<db.size(); i++) {

        Contact& localcontact = db.getContact(i) ;

        bool upload ;

        if (googlemaster) {
            // Google Master, So Upload by default
            upload=false ;
        } else {
            // Local Master, So Download by default
            upload=true ;
        }

        // A merged entry must reach Google, or the next download would put back its old details
        if (localcontact.isSet(Contact::MergePending)) upload=true ;

        // A deletion made locally must reach Google, whichever side is the master
        if (localcontact.isSet(Contact::Deleted) && !localcontact.getField(Contact::GoogleRecordId).isEmpty()) {
            upload=true ;

            // A merged-away entry is removed from Google only once the entry it was merged into has reached
            // Google, so that the merged details are never lost
            QString keptid = localcontact.getField(Contact::MergedInto) ;
            Contact &kept = db.getContactById(keptid) ;
            if (!keptid.isEmpty() && !kept.isNull() && kept.isSet(Contact::MergePending)) upload=false ;
        }

        localcontact.setFlag(Contact::ToBeDownloaded, !upload) ;
        localcontact.setFlag(Contact::ToBeUploaded, upload) ;
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 4. Fix broken ID links
    //
    setProgress(610) ;
    ui->updateStatusReport->append(QString("  Repairing Broken Local Google Record IDs:")) ;
    int size5=googledb.size() ;
    for (int i=0; i<size5; i++) {
        setProgress(610 + ( i * 10)/size5 ) ;
        Contact& googlecontact = googledb.getContact(i) ;
        Contact& localcontact = db.getContactById(googlecontact.getField(Contact::ID)) ;
        if (!localcontact.isNull() && !googlecontact.isSet(Contact::Deleted) &&
                localcontact.getField(Contact::GoogleRecordId).compare(googlecontact.getField(Contact::GoogleRecordId))!=0) {
            localcontact.setField(Contact::GoogleRecordId, googlecontact.getField(Contact::GoogleRecordId)) ;
            ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false,false)) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 5. Handle merges
    //
    setProgress(620) ;
    ui->updateStatusReport->append(QString("  Processing Merged Contacts:")) ;
    int size7 = googledb.size() ;
    for (int i=0; i<size7; i++) {

        setProgress(620 + ( i * 10)/size7 ) ;

        Contact& googlecontact = googledb.getContact(i) ;
        Contact& localcontact = db.getContactById(googlecontact.getField(Contact::ID)) ;

        if (!localcontact.isNull() && googlecontact.mergedIdCount()>1) {

            QStringList ids = googlecontact.mergedIdList() ;
            QString localid = localcontact.getField(Contact::ID) ;

            for (int i=0; i<ids.length(); i++) {

                QString id = ids.at(i) ;
                if (id.compare(localid)!=0) {

                    Contact& contact = db.getContactById(id) ;

                    if (contact.isSet(Contact::ToBeDownloaded)) {
                        // Merge all contacts together, then tag as deleted
                        contact.mergeInto(localcontact) ;
                        contact.setFlag(Contact::Deleted, true) ;
                        contact.setField(Contact::GoogleRecordId, QString("")) ;
                    } else {
                        // Remove record ID of merged contacts so that they will be re-uploaded as-if new later
                        contact.setField(Contact::GoogleRecordId, QString("")) ;
                    }
                }
            }

            // Update Google entry to remove additional IDs
            if (!localcontact.isEmpty()) {
                // Ensure contact has correct google ID
                localcontact.setField(Contact::GoogleRecordId, googlecontact.getField(Contact::GoogleRecordId)) ;
                QElapsedTimer uploadtimer ;
                uploadtimer.start() ;
                bool uploaded = uploadContact(localcontact, google, googlecontact) ;
                dbg(QString("TIMING upload (new local) %1 ms ok=%2").arg(uploadtimer.elapsed()).arg(uploaded)) ;
                if (uploaded) {
                    ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false,false)) ;
                } else {
                    ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false,false) + QString(" FAILED")) ;
                    networkok = false ;
                }
            }

        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 6. Add Fake entry in Google list for missing  contacts, and identify new local records
    //
    setProgress(630) ;
    PHASE_DONE("before identifying local contacts") ;
    ui->updateStatusReport->append(QString("  Identifying local contacts missing on Google:")) ;
    int size3 = db.size() ;

    // TIMING: where the time in this loop goes
    qint64 lookupns=0, addns=0 ;
    QElapsedTimer looptimer ;
    looptimer.start() ;

    for (int i=0; i<size3; i++) {
        setProgress(630 + ( i * 10)/size3 ) ;
        Contact& localcontact = db.getContact(i) ;

        QElapsedTimer lookuptimer ;
        lookuptimer.start() ;
        Contact& googlecontact = googledb.getContactById(localcontact.getField(Contact::ID)) ;
        lookupns += lookuptimer.nsecsElapsed() ;

        if (googlecontact.isNull()) {

            // Add fake 'deleted' entry to Google List
            Contact deletedcontact ;
            deletedcontact.setField(Contact::ID, localcontact.getField(Contact::ID)) ;
            deletedcontact.setFlag(Contact::Deleted, true) ;

            QElapsedTimer addtimer ;
            addtimer.start() ;
            googledb.addContact(deletedcontact) ;
            addns += addtimer.nsecsElapsed() ;

            if (localcontact.isSet(Contact::Hidden) || localcontact.isSet(Contact::Deleted)) {

                // Contact is hidden or deleted, so don't override any options
                // But, forget any previous Google Record ID
                localcontact.setField(Contact::GoogleRecordId, "") ;
                localcontact.setField(Contact::Uploaded, "") ;

            } else {

                if (localcontact.getField(Contact::GoogleRecordId).isEmpty()) {

                    // Contact has never been uploaded
                    // Override upload/download -> upload
                    localcontact.setFlag(Contact::ToBeUploaded, true) ;
                    localcontact.setFlag(Contact::ToBeDownloaded, false) ;
                    ui->updateStatusReport->append(QString("    %1 (new local)").arg(localcontact.getFormattedName(false,false))) ;

                } else {

                    // Contact has previously been uploaded
                    // So don't override options (stick with default)
                    // But, forget any previous Google Record ID
                    localcontact.setField(Contact::GoogleRecordId, "") ;
                    ui->updateStatusReport->append(QString("    %1 (removed google)").arg(localcontact.getFormattedName(false,false))) ;

                }
            }

        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 7. Add Fake entry in Local list for missing contacts, and identify new google records
    //
    setProgress(640) ;
    dbg(QString("TIMING identify local contacts: %1 entries, loop %2 ms, Google ID lookups %3 ms, adding fake Google entries %4 ms (other work %5 ms)")
        .arg(size3).arg(looptimer.elapsed()).arg(lookupns/1000000).arg(addns/1000000)
        .arg(looptimer.elapsed() - lookupns/1000000 - addns/1000000)) ;
    PHASE_DONE("identifying local contacts") ;
    ui->updateStatusReport->append(QString("  Identifying Google contacts missing locally:")) ;
    int size4 = googledb.size() ;
    for (int i=0; i<size4; i++) {
        setProgress(640 + ( i * 10)/size4 ) ;
        Contact& googlecontact = googledb.getContact(i) ;
        Contact& localcontact = db.getContactById(googlecontact.getField(Contact::ID)) ;
        if (localcontact.isNull() && !googlecontact.isSet(Contact::Deleted)) {
            ui->updateStatusReport->append(QString("    ") + googlecontact.getFormattedName(false,false)) ;
            Contact newlocal ;
            newlocal.setField(Contact::ID, googlecontact.getField(Contact::ID)) ;
            // Download (save) Google details.  Not uploaded: a contact made on Google has no ID stored there,
            // but its ID is derived from its resource name (see parseContact), so no upload is needed, and an
            // upload would rewrite fields this app does not hold, such as URLs
            newlocal.setFlag(Contact::ToBeDownloaded, true) ;
            db.addContact(newlocal) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 8. Update profile
    //
    setProgress(650) ;
    PHASE_DONE("identifying google contacts") ;
    ui->updateStatusReport->append(QString("  Storing Google Profile Changes:")) ;
    int size8 = db.size() ;
    for (int i=0; i<size8; i++) {
        setProgress(650 + ( i * 10)/size8 ) ;
        Contact& localcontact = db.getContact(i) ;
        Contact& googlecontact = googledb.getContactById(localcontact.getField(Contact::ID)) ;
        if ( !googlecontact.isEmpty() && !googlecontact.matches(localcontact, Contact::mcProfile)) {
            ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false, false)) ;
            googlecontact.copyTo(localcontact, Contact::mcProfile) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 9. Synchronise changes (download first)
    //
    setProgress(660) ;
    PHASE_DONE("storing profile changes") ;
    ui->updateStatusReport->append(QString("  Synchronising Changes:")) ;
    int size9 = db.size() ;
    QElapsedTimer synctimer ;
    synctimer.start() ;
    int uploadcount=0 ;
    qint64 uploadms=0 ;
    for (int i=0; i<size9; i++) {

        if (i % 50 == 0) {
            dbg(QString("TIMING sync contacts: %1/%2 items, %3 ms, %4 uploads so far (%5 ms uploading), log %6 chars")
                .arg(i).arg(size9).arg(synctimer.elapsed()).arg(uploadcount).arg(uploadms).arg(getLog().size())) ;
        }

        setProgress(660 + ( i * 240)/size9 ) ;
        if (state<0) return false ;

        Contact& localcontact = db.getContact(i) ;
        Contact& googlecontact = googledb.getContactById(localcontact.getField(Contact::ID)) ;

        bool deletedlocaldeletedgoogle =
                (googlecontact.isNull() || googlecontact.isSet(Contact::Deleted)) &&
                (localcontact.isSet(Contact::Hidden) || localcontact.isSet(Contact::Deleted)) ;
        // The Deleted flag is not part of the comparison, so a local deletion that
        // Google still has as a live record must not be treated as a match
        bool localdeleted = localcontact.isSet(Contact::Deleted) ;
        bool googleremoved = googlecontact.isNull() || googlecontact.isSet(Contact::Deleted) ;
        bool googlematcheslocal = !(localdeleted && !googleremoved) &&
                googlecontact.matches(localcontact, Contact::mcDetails|Contact::mcId) ;

        // A merged entry that already matches Google needs no upload
        if (googlematcheslocal && localcontact.isSet(Contact::MergePending)) {
            localcontact.setFlag(Contact::MergePending, false) ;
        }

        if ( deletedlocaldeletedgoogle ) {

            // Deleted everywhere, so nothing is required.  Google has no live record,
            // so the link is forgotten; the entry is then archived the next time it loads.
            // Only done when the download succeeded, otherwise "missing" means "unknown".
            if (networkok && !localcontact.getField(Contact::GoogleRecordId).isEmpty()) {
                localcontact.setField(Contact::GoogleRecordId, QString("")) ;
                localcontact.setField(Contact::GoogleEtag, QString("")) ;
            }
            localcontact.setFlag(Contact::ToBeUploaded, false) ;
            localcontact.setFlag(Contact::ToBeDownloaded, false) ;

        } else if (!googlematcheslocal) {

            QString changes ;
            QString summary ;

            // Download Details from Google
            if (localcontact.isSet(Contact::ToBeDownloaded)) {
                if (googlecontact.isSet(Contact::Deleted)) {
                    changes = QString("Deleted.") ;
                    summary = QString("deleted") ;
                    localcontact.setFlag(Contact::Deleted, true) ;
                } else if (googlecontact.isSet(Contact::Hidden)) {
                    changes = QString("Hidden.") ;
                    summary = QString("hidden") ;
                    localcontact.setFlag(Contact::Hidden, true) ;
                } else {
                    changes = QString("Downloaded: ") + googlecontact.mismatch(localcontact, Contact::mcDetailsGroup|Contact::mcId) + QString(". ");
                    summary = QString("downloaded") ;
                    googlecontact.copyTo(localcontact, Contact::mcDetailsGroup|Contact::mcGoogleId) ;
                }
                ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false, false)  + QString(" - Updating Local Contact Details (%1)").arg(summary)) ;
            }

            // Update Details on Google
            if (localcontact.isSet(Contact::ToBeUploaded)) {
                if (localcontact.isSet(Contact::Deleted)) {
                    changes = QString("Deleted.") ;
                    summary = QString("deleted") ;
                } else if (localcontact.isSet(Contact::Hidden)) {
                    changes = QString("Hidden.") ;
                    summary = QString("hidden") ;
                } else {
                    changes = QString("Uploaded: ") + localcontact.mismatch(googlecontact, Contact::mcDetails|Contact::mcId) + QString(". ");
                    summary = QString("uploaded") ;
                }
                QElapsedTimer uploadtimer ;
                uploadtimer.start() ;
                bool uploaded = uploadContact(localcontact, google, googlecontact) ;
                uploadcount++ ;
                uploadms += uploadtimer.elapsed() ;
                dbg(QString("TIMING upload #%1 %2 ms ok=%3").arg(uploadcount).arg(uploadtimer.elapsed()).arg(uploaded)) ;
                if (uploaded) {
                    ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false, false)  + QString(" - Updating Google Contact Details (%1)").arg(summary)) ;
                    localcontact.copyTo(googlecontact, Contact::mcDetails|Contact::mcId|Contact::mcGoogleId) ;
                    localcontact.setFlag(Contact::MergePending, false) ;
                } else {
                    // The reason is the network error if there was one, otherwise the last log entry, which says
                    // what did not match (for example, after Google returned the contact)
                    QStringList recent = QString(getLog()).split("\n", Qt::SkipEmptyParts) ;
                    QString reason = google.getNetworkError().isEmpty() ? (recent.isEmpty() ? QString() : recent.last().trimmed()) : google.getNetworkError() ;
                    ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false, false)  + QString(" - FAILED Updating Google Contact Details: ") + reason) ;
                    networkok = false ;
                }

                // If the record has been successfully removed from Google, forget the local details
                if (uploaded && (localcontact.isSet(Contact::Deleted) || localcontact.isSet(Contact::Hidden))) {
                    localcontact.setField(Contact::GoogleRecordId, QString("")) ;
                    localcontact.setField(Contact::GoogleEtag, QString("")) ;
                    localcontact.setFlag(Contact::ToBeUploaded, false) ;
                }
            }

            if (!changes.isEmpty()) {
                localcontact.getHistory().addEntry(changes) ;
            }

        }

    }

    if (state<0) return false ;

    // 10. Synchronise Groups
    setProgress(900) ;

    if (networkok) {
        // Upload record groups
        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupBusiness, db, googledb) ; }
        setProgress(918) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupClient, db, googledb) ; }
        setProgress(936) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupFamily, db, googledb) ; }
        setProgress(954) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupFriend, db, googledb) ; }
        setProgress(972) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupOther, db, googledb) ; }
        setProgress(990) ;
        if (state<0) return false ;

    }


    if (networkok) {
        // Download record groups
        updateSingleLocalContactGroup(google, Contact::GroupBusiness, db, googledb) ;
        setProgress(992) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupClient, db, googledb) ;
        setProgress(994) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupFamily, db, googledb) ;
        setProgress(996) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupFriend, db, googledb) ;
        setProgress(998) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupOther, db, googledb) ;
        setProgress(1000) ;
        if (state<0) return false ;

    }


    PHASE_DONE("synchronising changes") ;
    dbg(QString("TIMING sync contacts done: %1 uploads, %2 ms uploading, %3 ms in sync loop")
        .arg(uploadcount).arg(uploadms).arg(synctimer.elapsed())) ;
    setProgress(1000) ;
    if (networkok) {
        ui->updateStatusReport->append(QString("  Contact Synchronisation: OK")) ;
    } else {
        ui->updateStatusReport->append(QString("  Contact Synchronisation: FAILED - Please resync")) ;
    }

    return networkok ;
}


// Upload group to Google
bool GoogleUpdateDialog::updateSingleGoogleContactGroup(GoogleAccess &google, Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb)
{
    ui->updateStatusReport->append(QString("  Updating Google %1 Group").arg(google.getGroupId(rec, true))) ;
    bool networkok=google.updateSingleGoogleContactGroup(rec, db, googledb) ;
    QString addedstr = google.contactsAddedToGroup() ;
    QString deletedstr = google.contactsDeletedFromGroup() ;
    if (!addedstr.isEmpty()) ui->updateStatusReport->append(addedstr) ;
    if (!deletedstr.isEmpty()) ui->updateStatusReport->append(deletedstr) ;
    return networkok ;
}

// 'Download' group from Google
bool GoogleUpdateDialog::updateSingleLocalContactGroup(GoogleAccess &google, Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb)
{
    QString addedstr, deletedstr ;

    ui->updateStatusReport->append(QString("  Updating Local %1 Group").arg(google.getGroupId(rec, true))) ;

    for (int i=0; i<googledb.size(); i++) {

        Contact& googlecontact = googledb.getContact(i) ;
        Contact& localcontact = db.getContactById(googlecontact.getField(Contact::ID)) ;

        // Do transfer if valid entry and entry exists on google, and transfer requested
        bool dotransfer = true ;
        if (localcontact.isNull()) dotransfer=false ;
        if (googlecontact.getField(Contact::GoogleRecordId).isEmpty()) dotransfer=false ;
        if (!localcontact.isSet(Contact::ToBeDownloaded)) dotransfer=false ;

        if (dotransfer) {

            bool googlegrp = googlecontact.isSet(rec) ;
            bool localgrp = localcontact.isSet(rec) ;

            if (googlegrp!=localgrp) {
                if (googlegrp) {
                    localcontact.setFlag(rec, true) ;
                    if (!addedstr.isEmpty()) addedstr = addedstr + "\n" ;
                    addedstr = addedstr + "   + " + googlecontact.getFormattedName(false, false) ;
                } else {
                    localcontact.setFlag(rec, false) ;
                    if (!deletedstr.isEmpty()) deletedstr = deletedstr + "\n";
                    deletedstr = deletedstr + "   - " + googlecontact.getFormattedName(false, false) ;
                }
            }
            if (state<0) return false ;
        }
    }

    if (!addedstr.isEmpty()) ui->updateStatusReport->append(addedstr) ;
    if (!deletedstr.isEmpty()) ui->updateStatusReport->append(deletedstr) ;

    return true ;
}
