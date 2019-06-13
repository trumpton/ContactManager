
//
// googleupdatedialog_contacts.cpp
//


#include "googleupdatedialog.h"
#include "ui_googleupdatedialog.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"

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


bool GoogleUpdateDialog::uploadContact(Contact& contact, GoogleAccess &google, QString etag)
{
    bool success=false ;

    QString& googleid = contact.getField(Contact::GoogleRecordId) ;

    if (googleid.isEmpty() || etag.isEmpty()) {
        success = google.updateContact(contact, GoogleAccess::Post) ;
    } else {
        success = google.updateContact(contact, GoogleAccess::Patch, etag) ;
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
    ui->progressBar->setValue(500) ;
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
    ui->progressBar->setValue(550) ;
    networkok &= google.getContacts(googledb) ;
    ui->updateStatusReport->append(QString("  Downloaded ") + QString::number(googledb.size()) + QString(" Contacts from Google.")) ;

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 3. Set the default upload / download
    //
    ui->progressBar->setValue(600) ;
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

        localcontact.setFlag(Contact::ToBeDownloaded, !upload) ;
        localcontact.setFlag(Contact::ToBeUploaded, upload) ;
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 4. Fix broken ID links
    //
    ui->progressBar->setValue(610) ;
    ui->updateStatusReport->append(QString("  Repairing Broken Local Google Record IDs:")) ;
    int size5=googledb.size() ;
    for (int i=0; i<size5; i++) {
        ui->progressBar->setValue(610 + ( i * 10)/size5 ) ;
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
    ui->progressBar->setValue(620) ;
    ui->updateStatusReport->append(QString("  Processing Merged Contacts:")) ;
    int size7 = googledb.size() ;
    for (int i=0; i<size7; i++) {

        ui->progressBar->setValue(620 + ( i * 10)/size7 ) ;

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
                if (uploadContact(localcontact, google, googlecontact.getField(Contact::GoogleEtag))) {
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
    ui->progressBar->setValue(630) ;
    ui->updateStatusReport->append(QString("  Identifying local contacts missing on Google:")) ;
    int size3 = db.size() ;
    for (int i=0; i<size3; i++) {
        ui->progressBar->setValue(630 + ( i * 10)/size3 ) ;
        Contact& localcontact = db.getContact(i) ;
        Contact& googlecontact = googledb.getContactById(localcontact.getField(Contact::ID)) ;

        if (googlecontact.isNull()) {

            // Add fake 'deleted' entry to Google List
            Contact deletedcontact ;
            deletedcontact.setField(Contact::ID, localcontact.getField(Contact::ID)) ;
            deletedcontact.setFlag(Contact::Deleted, true) ;
            googledb.addContact(deletedcontact) ;

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
    ui->progressBar->setValue(640) ;
    ui->updateStatusReport->append(QString("  Identifying Google contacts missing locally:")) ;
    int size4 = googledb.size() ;
    for (int i=0; i<size4; i++) {
        ui->progressBar->setValue(640 + ( i * 10)/size4 ) ;
        Contact& googlecontact = googledb.getContact(i) ;
        Contact& localcontact = db.getContactById(googlecontact.getField(Contact::ID)) ;
        if (localcontact.isNull() && !googlecontact.isSet(Contact::Deleted)) {
            ui->updateStatusReport->append(QString("    ") + googlecontact.getFormattedName(false,false)) ;
            Contact newlocal ;
            newlocal.setField(Contact::ID, googlecontact.getField(Contact::ID)) ;
            // Download (save) Google details
            newlocal.setFlag(Contact::ToBeDownloaded, true) ;
            // Re-upload to affix ID if it was not already available on Google
            if (!googlecontact.isSet(Contact::Uploaded)) newlocal.setFlag(Contact::ToBeUploaded, true) ;
            db.addContact(newlocal) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 8. Update profile
    //
    ui->progressBar->setValue(650) ;
    ui->updateStatusReport->append(QString("  Storing Google Profile Changes:")) ;
    int size8 = db.size() ;
    for (int i=0; i<size8; i++) {
        ui->progressBar->setValue(650 + ( i * 10)/size8 ) ;
        Contact& localcontact = db.getContact(i) ;
        Contact& googlecontact = googledb.getContactBy(Contact::ID, localcontact.getField(Contact::ID)) ;
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
    ui->progressBar->setValue(660) ;
    ui->updateStatusReport->append(QString("  Synchronising Changes:")) ;
    int size9 = db.size() ;
    for (int i=0; i<size9; i++) {

        ui->progressBar->setValue(660 + ( i * 240)/size9 ) ;
        if (state<0) return false ;

        Contact& localcontact = db.getContact(i) ;
        Contact& googlecontact = googledb.getContactBy(Contact::ID, localcontact.getField(Contact::ID)) ;

        bool deletedlocaldeletedgoogle =
                (googlecontact.isNull() || googlecontact.isSet(Contact::Deleted)) &&
                (localcontact.isSet(Contact::Hidden) || localcontact.isSet(Contact::Deleted)) ;
        bool googlematcheslocal = googlecontact.matches(localcontact, Contact::mcDetails|Contact::mcId) ;

        if ( deletedlocaldeletedgoogle ) {

            // Deleted everywhere, so nothing is required
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
                if (uploadContact(localcontact, google, googlecontact.getField(Contact::GoogleEtag))) {
                    ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false, false)  + QString(" - Updating Google Contact Details (%1)").arg(summary)) ;
                    localcontact.copyTo(googlecontact, Contact::mcDetails|Contact::mcId|Contact::mcGoogleId) ;
                } else {
                    ui->updateStatusReport->append(QString("    ") + localcontact.getFormattedName(false, false)  + QString(" - FAILED Updating Google Contact Details")) ;
                    networkok = false ;
                }

                // If the record has been successfully removed from Google, forget the local details
                if (networkok && (localcontact.isSet(Contact::Deleted) || localcontact.isSet(Contact::Hidden))) {
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
    ui->progressBar->setValue(900) ;

    if (networkok) {
        // Upload record groups
        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupBusiness, db, googledb) ; }
        ui->progressBar->setValue(918) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupClient, db, googledb) ; }
        ui->progressBar->setValue(936) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupFamily, db, googledb) ; }
        ui->progressBar->setValue(954) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupFriend, db, googledb) ; }
        ui->progressBar->setValue(972) ;
        if (state<0) return false ;

        if (networkok) { networkok = updateSingleGoogleContactGroup(google, Contact::GroupOther, db, googledb) ; }
        ui->progressBar->setValue(990) ;
        if (state<0) return false ;

    }


    if (networkok) {
        // Download record groups
        updateSingleLocalContactGroup(google, Contact::GroupBusiness, db, googledb) ;
        ui->progressBar->setValue(992) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupClient, db, googledb) ;
        ui->progressBar->setValue(994) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupFamily, db, googledb) ;
        ui->progressBar->setValue(996) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupFriend, db, googledb) ;
        ui->progressBar->setValue(998) ;
        if (state<0) return false ;

        updateSingleLocalContactGroup(google, Contact::GroupOther, db, googledb) ;
        ui->progressBar->setValue(1000) ;
        if (state<0) return false ;

    }


    ui->progressBar->setValue(1000) ;
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
