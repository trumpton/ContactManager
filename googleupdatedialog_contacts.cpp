
//
// googleupdatedialog_contacts.cpp
//


#include "googleupdatedialog.h"
#include "ui_googleupdatedialog.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"



bool GoogleUpdateDialog::processContactUpdate(QDateTime &lastsync, GoogleAccess& google, class ContactDatabase& db)
{
    bool networkok = true ;

    ContactDatabase googledb ;
    QString gacct = gConf->getGoogleUsername().replace("@","%40") ;

    ui->updateStatusReport->append(QString("  Downloading New and Changed Contacts from Google")) ;

    // DOWNLOAD GOOGLE CHANGES SINCE LASTSYNC INC DELETED & EMPTY
    networkok &= google.getContacts(googledb, 1) ;
    int numchangedongoogle = googledb.size() ;
    for (int i=0; i<numchangedongoogle; i++) {
        Contact &googlecontact = googledb.getContact(i) ;
        QString txt = googlecontact.asText() ;
        ui->updateStatusReport->append(QString("    Downloading: ") + txt) ;
    }
    ui->updateStatusReport->append(QString("    Downloaded: ") + QString::number(numchangedongoogle) + QString(" entries")) ;


    // DOWNLOAD GOOGLE ENTRIES FOR CHANGED LOCAL ENTRIES NOT ALREADY DOWNLOADED
    if (networkok && state>0) {
       ui->progressBar->setValue(ui->progressBar->value()+100) ;
       ui->updateStatusReport->append(QString("  Downloading Google Contacts for Changed Local Entries")) ;
       for (int i=0, dsz=db.size(); networkok && state>0 && i<dsz; i++) {
            Contact &localcontact = db.getContact(i) ;
             bool isupdated = localcontact.getDate(Contact::Updated)>=lastsync ;
             bool ongoogle = localcontact.isForAccount(gacct) ;
             QString googlerecordid = localcontact.getField(Contact::GoogleRecordId) ;
             bool ingoogledb = !googledb.getContactBy(Contact::GoogleRecordId, googlerecordid).isNull() ;
             if (isupdated && ongoogle && !ingoogledb) {
                 Contact googlecontact = localcontact ;
                 if (google.getContact(googlecontact)) {
                     // Downloaded contact
                     googledb.addContact(googlecontact) ;
                     ui->updateStatusReport->append(QString("    Downloading: ") + googlecontact.asText()) ;
                 } else if (google.getNetworkErrorCode()==404) {
                     // 404 is returned if the remote contact has been purged, so create a fake entry
                     Contact purgedcontact ;
                     purgedcontact.setField(Contact::ID, localcontact.getField(Contact::ID)) ;
                     purgedcontact.setField(Contact::GoogleRecordId, localcontact.getField(Contact::GoogleRecordId)) ;
                     purgedcontact.setFlag(Contact::Deleted, true) ;
                     purgedcontact.setFlag(Contact::GoogleDeleted, true) ;
                     googledb.addContact(purgedcontact) ;
                     ui->updateStatusReport->append(QString("    Purged: ") + googlecontact.asText()) ;
                 } else {
                     // Any other responses are fatal
                     networkok = false ;
                }
             }
         }
       ui->updateStatusReport->append(QString("    Downloaded: ") + QString::number(googledb.size() - numchangedongoogle) + QString(" entries")) ;
    }

    // LOCALLY STORE NEW NON-DELETED ENTRIES THAT AREN'T ALREADY KNOWN
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        ui->updateStatusReport->append(QString("  Storing and Refreshing New Google Contacts")) ;
        for (int i=0, gsz=googledb.size(); networkok && state>0 && i<gsz; i++) {
            Contact &googlecontact = googledb.getContact(i) ;
            QString googlecontactid = googlecontact.getGoogleRecordId() ;
            bool isdeleted = googlecontact.isSet(Contact::GoogleDeleted);
            bool indb = !db.getContactBy(Contact::GoogleRecordId, googlecontactid).isNull() ;
            if (!isdeleted && !indb) {
                // TODO: Check how Appointments achieves this (i.e. saves files)
                googlecontact.markAsDirty() ;
                if (!googlecontact.isSet(Contact::GoogleUploaded)) {
                    googlecontact.history.addEntry("Downloaded from Google, and re-uploaded") ;
                    ui->updateStatusReport->append(QString("    Storing: ") + googlecontact.asText() + QString(" + re-uploading")) ;
                    networkok &= google.updateContact(googlecontact, GoogleAccess::Update) ;
                } else {
                    googlecontact.history.addEntry("Downloaded from Google") ;
                    ui->updateStatusReport->append(QString("    Storing: ") + googlecontact.asText()) ;
                }
                db.addContact(googlecontact) ;
            }
        }
    }


    // UPLOAD LOCAL ENTRIES FOR NEW NON-DELETED LOCAL ENTRIES NOT ALREADY ON GOOGLE
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        ui->updateStatusReport->append(QString("  Uploading New Local Entries")) ;
        for (int i=0, dsz=db.size(); networkok && state>0 && i<dsz; i++) {
            Contact &localcontact = db.getContact(i) ;
            bool isdeleted = localcontact.isSet(Contact::Deleted) ;
            bool ongoogle = localcontact.isForAccount(gacct) ;
            bool isnew = !localcontact.isSet(Contact::GoogleUploaded) ;
            if (!isdeleted && (!ongoogle || isnew)) {
                networkok &= google.updateContact(localcontact, GoogleAccess::Create) ;
                googledb.addContact(localcontact) ;
                ui->updateStatusReport->append(QString("    Uploading ") + localcontact.asText()) ;
                localcontact.history.addEntry("Uploaded to Google") ;
            }
        }
    }

    // WALK THROUGH THE GOOGLE LIST
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        ui->updateStatusReport->append(QString("  Synchronising Changes")) ;
        for (int i=0, gsz=googledb.size(); networkok && state>0 && i<gsz; i++) {

            Contact& googlecontact = googledb.getContact(i) ;
            QString id = googlecontact.getGoogleRecordId() ;
            Contact& localcontact = db.getContactBy(Contact::GoogleRecordId, id) ;

            // WORK OUT WHAT HAS CHANGED
            bool localchanged = (localcontact.getDate(Contact::Updated) > lastsync) ;
            bool same = localcontact.matches(googlecontact) ;
            bool deletingnonexistantentry = googlecontact.isSet(Contact::Deleted) && localcontact.isNull() ;

            // SYNC CHANGES PROVIDED ENTRY HASN'T BEEN REMOVED
            if (!localcontact.isNull() && !same && !deletingnonexistantentry) {

                // UPLOAD = (LOCALMASTER || LATESTMASTER_LOCALPREF && LOCALCHANGED)
                bool upload = (gConf->contactManagerMaster() || (gConf->latestMaster() && localchanged)) ;

                // CALCULATE AND LOG CHANGED DETAILS
                QString changelog ;
                for (int j=Contact::FIRSTSYNCEDRECORD; j<=Contact::LASTSYNCEDRECORD;j++) {
                    QString l = localcontact.getField((Contact::ContactRecord)j).replace("\n"," ") ;
                    QString g = googlecontact.getField((Contact::ContactRecord)j).replace("\n", " ") ;
                    QString f = localcontact.getContactRecordName((Contact::ContactRecord)j) ;
                    if (l.compare(g)!=0) {
                        if (!changelog.isEmpty()) changelog = changelog + "; " ;
                        if (upload) {
                            changelog = changelog + "'" + f + "' from '" + g + "' to '" + l + "'";
                        } else {
                            changelog = changelog + "'" + f + "' from '" + l + "' to '" + g + "'";
                        }
                    }
                }

                // IF UPLOAD THEN UPLOAD ENTRY
                if (upload) {
                    if (localcontact.isSet(Contact::Deleted) && (googlecontact.isSet(Contact::Deleted))) {
                        // Do nothing - everything has already been removed
                    } else if (!localcontact.isSet(Contact::Deleted) && googlecontact.isSet(Contact::Deleted)) {
                        // Google was deleted so upload a new copy
                        googlecontact.copyGoogleAccountFieldsTo(localcontact) ;
                        networkok &= google.updateContact(localcontact, GoogleAccess::Create) ;
                        ui->updateStatusReport->append(QString("    Uploading: ") + localcontact.asText()) ;
                    } else if (localcontact.isSet(Contact::Deleted)) {
                        // Remove google version
                        networkok &= google.updateContact(localcontact, GoogleAccess::Delete) ;
                        ui->updateStatusReport->append(QString("    Deleting Google: ") + localcontact.asText()) ;
                    } else {
                        // Update google version
                        googlecontact.copyGoogleAccountFieldsTo(localcontact) ;
                        networkok &= google.updateContact(localcontact, GoogleAccess::Update) ;
                        ui->updateStatusReport->append(QString("    Uploading: ") + localcontact.asText()) ;
                    }
                    localcontact.history.addEntry("Changed Uploaded to Google: " + changelog) ;
                }

                // ELSE STORE DOWNLOADED ENTRY
                else {
                        if (googlecontact.isSet(Contact::Deleted)) {
                            ui->updateStatusReport->append(QString("    Deleting Local: ") + localcontact.asText()) ;
                        } else {
                            ui->updateStatusReport->append(QString("    Storing: ") + googlecontact.asText()) ;
                        }
                        googlecontact.copySyncedFieldsTo(localcontact) ;
                        googlecontact.copyGoogleAccountFieldsTo(localcontact) ;
                        localcontact.history.addEntry("Changes Downloaded from Google: " + changelog) ;
                }

            }

        }
    }


    // SYNCHRONISE TOKENS
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        bool unchanged = true ;
        googledb.clear() ;
        networkok &= google.getContacts(googledb, 2) ;
        for (int i=0, gsz=googledb.size(); i<gsz; i++) {
            Contact &googlecontact = googledb.getContact(i) ;
            Contact &localcontact = db.getContactBy(Contact::GoogleRecordId, googlecontact.getField(Contact::GoogleRecordId)) ;
            if (!googlecontact.matches(localcontact)) unchanged=false ;
        }
        if (unchanged) {
            ui->updateStatusReport->append(QString("  Verifying synchronisation: OK")) ;
        } else {
            ui->updateStatusReport->append(QString("  Verifying synchronisation: FAILED - Please resync")) ;
            networkok = false ;
        }
    }

    return networkok ;
}
