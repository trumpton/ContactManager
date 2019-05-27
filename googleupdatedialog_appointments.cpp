//
// googleupdatedialog_appointments.cpp
//

#include "googleupdatedialog.h"
#include "ui_googleupdatedialog.h"
#include "configuration.h"

bool GoogleUpdateDialog::processCalendarUpdate(QDateTime &lastsync, GoogleAccess& google, class Calendar& cal)
{
    bool networkok = true ;

    Calendar googlecal ;
    QString gacct = gConf->getGoogleUsername().replace("@","%40") ;

    ui->updateStatusReport->append(QString("  Downloading New and Changed Appointments from Google")) ;

    // DOWNLOAD GOOGLE CHANGES SINCE LASTSYNC INC DELETED AND PURGED
    networkok &= google.getCalendar(googlecal) ;
    int numchangedongoogle = googlecal.size() ;
    for (int i=0; i<numchangedongoogle; i++) {
        Appointment &googleappt = googlecal.getAppointment(i) ;
        ui->updateStatusReport->append(QString("    Downloading: ") + googleappt.asText()) ;
    }
    ui->updateStatusReport->append(QString("    Downloaded: ") + QString::number(numchangedongoogle) + QString(" entries")) ;

    if (state<0) return false ;

    // DOWNLOAD GOOGLE ENTRIES FOR CHANGED LOCAL ENTRIES NOT ALREADY DOWNLOADED
    if (networkok && state>0) {
       ui->progressBar->setValue(ui->progressBar->value()+100) ;
       ui->updateStatusReport->append(QString("  Downloading Google Appointments for Changed Local Entries")) ;
       for (int i=0, csz=cal.size(); networkok && state>0 && i<csz; i++) {
             Appointment &localappt = cal.getAppointment(i) ;
             bool isupdated = localappt.getDate(Appointment::Updated)>=lastsync ;
             bool ongoogle = localappt.isForAccount(gacct) ;
             QString googlerecordid = localappt.getField(Appointment::GoogleRecordId) ;
             bool ingooglecal = !googlecal.getAppointmentBy(Appointment::GoogleRecordId, googlerecordid).isNull() ;
             if (isupdated && ongoogle && !ingooglecal) {
                 Appointment googleappt = localappt ;
                 networkok &= google.getAppointment(googleappt) ;
                 googlecal.addAppointment(googleappt) ;
                 ui->updateStatusReport->append(QString("    Downloading: ") + googleappt.asText()) ;
             }
             if (state<0) return false ;
         }
       ui->updateStatusReport->append(QString("    Downloaded: ") + QString::number(googlecal.size() - numchangedongoogle) + QString(" entries")) ;
    }

    if (state<0) return false ;

    // TODO: Purged Google Entries will appear as Deleted, without any Extended Properties
    // Need this if we are to support an 'unpurge' (create a new entry), or 'delete' - remove
    // the corresponding local entry.
    // In the meanwhile, don't empty the bin on Google!!!!

    // LOCALLY STORE NEW NON-DELETED ENTRIES THAT AREN'T ALREADY KNOWN
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        ui->updateStatusReport->append(QString("  Storing New Google Appointments")) ;
        for (int i=0, gsz=googlecal.size(); networkok && state>0 && i<gsz; i++) {
            Appointment &googleappt = googlecal.getAppointment(i) ;
            QString googleapptid = googleappt.getGoogleRecordId() ;
            bool isdeleted = googleappt.isSet(Appointment::Deleted) ;
            bool incalendar = !cal.getAppointmentBy(Appointment::GoogleRecordId, googleapptid).isNull() ;
            if (!isdeleted && !incalendar) {
                cal.addAppointment(googleappt) ;
                ui->updateStatusReport->append(QString("    Storing: ") + googleappt.asText()) ;
            }
            if (state<0) return false ;
        }
    }

    if (state<0) return false ;

    // UPLOAD LOCAL ENTRIES FOR NEW NON-DELETED LOCAL ENTRIES NOT ALREADY ON GOOGLE
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        ui->updateStatusReport->append(QString("  Uploading New Local Entries")) ;
        for (int i=0, csz=cal.size(); networkok && state>0 && i<csz; i++) {
            Appointment &localappt = cal.getAppointment(i) ;
            bool isdeleted = localappt.isSet(Appointment::Deleted) ;
            bool ongoogle = localappt.isForAccount(gacct) ;
            if (!isdeleted && !ongoogle) {
                networkok &= google.updateAppointment(localappt, GoogleAccess::Post) ;
                googlecal.addAppointment(localappt) ;
                ui->updateStatusReport->append(QString("    Uploading ") + localappt.asText()) ;
            }
            if (state<0) return false ;
        }
    }

    if (state<0) return false ;

    // WALK THROUGH THE LOCAL LIST
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        ui->updateStatusReport->append(QString("  Synchronising Changes")) ;
        for (int i=0, gsz=googlecal.size(); networkok && state>0 && i<gsz; i++) {

            if (state<0) return false ;

            Appointment& googleappt = googlecal.getAppointment(i) ;
            QString id = googleappt.getGoogleRecordId() ;
            Appointment& localappt = cal.getAppointmentBy(Appointment::GoogleRecordId, id) ;

            // WORK OUT WHAT HAS CHANGED
            bool localchanged = (localappt.getDate(Appointment::Updated) > lastsync) ;
            bool same = localappt.matches(googleappt) ;
            bool deletingnonexistantentry = googleappt.isSet(Appointment::Deleted) && localappt.isNull() ;

            // SYNC CHANGES PROVIDED ENTRY HASN'T BEEN PURGED
            if (!same && !deletingnonexistantentry) {

                // UPLOAD = (LOCALMASTER || LATESTMASTER_LOCALPREF && LOCALCHANGED)
                bool upload = (gConf->contactManagerMaster()) ;

                // IF UPLOAD THEN UPLOAD ENTRY
                if (upload) {
                        googleappt.copyGoogleAccountFieldsTo(localappt) ;
                        networkok &= google.updateAppointment(localappt, GoogleAccess::Put) ;
                        ui->updateStatusReport->append(QString("    Uploading: ") + localappt.asText()) ;
                }

                // ELSE STORE DOWNLOADED ENTRY
                else {
                        googleappt.copySyncedFieldsTo(localappt) ;
                        googleappt.copyGoogleAccountFieldsTo(localappt) ;
                        ui->updateStatusReport->append(QString("    Storing: ") + localappt.asText()) ;
                }

            }

        }
    }

    if (state<0) return false ;

    // SYNCHRONISE TOKENS
    if (networkok && state>0) {
        ui->progressBar->setValue(ui->progressBar->value()+100) ;
        bool unchanged = true ;
        googlecal.clear() ;
        networkok &= google.getCalendar(googlecal) ;
        for (int i=0, gsz=googlecal.size(); i<gsz; i++) {
            Appointment &googleappt = googlecal.getAppointment(i) ;
            Appointment &localappt = cal.getAppointmentBy(Appointment::GoogleRecordId, googleappt.getField(Appointment::GoogleRecordId)) ;
            if (!googleappt.matches(localappt)) unchanged=false ;
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
