//
// googleupdatedialog_appointments.cpp
//

#include <QDateTime>
#include "googleupdatedialog.h"
#include "ui_googleupdatedialog.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"

bool GoogleUpdateDialog::processCalendarUpdate(QDateTime &lastsync, GoogleAccess& google, class Calendar& cal)
{
    Q_UNUSED(lastsync) ;

    bool networkok = true ;
    QDateTime fromwindow = QDateTime::currentDateTimeUtc().addDays(-1*APPOINTMENTSTARTWINDOW).addSecs(3600) ;
    QDateTime towindow = QDateTime::currentDateTimeUtc().addDays(APPOINTMENTENDWINDOW).addSecs(-3600) ;
    Calendar googlecal ;

    // ------------------------------------------------------------------------------------
    //
    // 1. Download Google Calendar List (all, inc purged)
    //

    ui->updateStatusReport->append(QString("  Downloading New and Changed Appointments from Google:")) ;
    ui->progressBar->setValue(0) ;
    networkok &= google.getCalendar(googlecal, APPOINTMENTSTARTWINDOW, APPOINTMENTENDWINDOW) ;
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
    ui->progressBar->setValue(100) ;
    bool googlemaster = gConf->googleMaster() ;

    for (int i=0; i<cal.size(); i++) {

        Appointment& localappt = cal.getAppointment(i) ;

        bool upload ;

        if (googlemaster) {
            // Google Master, So Upload by default
            upload=false ;
        } else {
            // Local Master, So Download by default
            upload=true ;
        }

        localappt.setFlag(Appointment::ToBeDownloaded, !upload) ;
        localappt.setFlag(Appointment::ToBeUploaded, upload) ;
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 3. Fix broken ID links
    //
    ui->progressBar->setValue(150) ;
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
    ui->progressBar->setValue(200) ;
    ui->updateStatusReport->append(QString("  Identifying local appointments missing on Google:")) ;

    for (int i=0, size=cal.size(); i<size; i++) {

        Appointment& localappt = cal.getAppointment(i) ;
        QDateTime from = localappt.getDate(Appointment::From) ;
        QDateTime to = localappt.getDate(Appointment::To) ;

        // Just process non-deleted entries within sync window
        if ( !localappt.isSet(Appointment::Deleted) &&
              ( (from <= fromwindow && to >= fromwindow) ||
                (from >= fromwindow && to <= towindow) ||
                ( from <= towindow && to >= towindow) ) ) {

            Appointment& googleappt = googlecal.getAppointmentBy(Appointment::ID, localappt.getField(Appointment::ID)) ;

            if (googleappt.isNull()) {

                Appointment deletedappt ;
                deletedappt.setField(Appointment::ID, localappt.getField(Appointment::ID)) ;
                deletedappt.setFlag(Appointment::Deleted, true) ;
                googlecal.addAppointment(deletedappt) ;
                ui->updateStatusReport->append(QString("        ") + googleappt.asText()) ;

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
    ui->progressBar->setValue(250) ;
    ui->updateStatusReport->append(QString("  Identifying Google appointments missing locally:")) ;
    for (int i=0, size=googlecal.size(); i<size; i++) {

        Appointment& googleappt = googlecal.getAppointment(i) ;
        Appointment& localappt = cal.getAppointmentBy(Appointment::ID, googleappt.getField(Appointment::ID)) ;

        if (localappt.isNull() && !googleappt.isSet(Appointment::Deleted)) {
            Appointment newlocal ;
            newlocal.setField(Appointment::ID, googleappt.getField(Appointment::ID)) ;
            newlocal.setField(Appointment::From, googleappt.getField(Appointment::From)) ;
            newlocal.setField(Appointment::To, googleappt.getField(Appointment::To)) ;
            newlocal.setField(Appointment::Summary, QString("--! Undefined !--")) ;

            newlocal.setFlag(Appointment::ToBeDownloaded, true) ;
            newlocal.setFlag(Appointment::ToBeUploaded, true) ;
            cal.addAppointment(newlocal) ;
            ui->updateStatusReport->append(QString("        ") + googleappt.asText()) ;
        }
    }

    if (state<0) return false ;

    // ------------------------------------------------------------------------------------
    //
    // 6. Synchronise changes (download first)
    //

    ui->progressBar->setValue(300) ;
    ui->updateStatusReport->append(QString("  Synchronising Changes:")) ;

    for (int i=0, size=cal.size(); i<size; i++) {

        ui->progressBar->setValue(300 + ( i * 200)/size ) ;
        if (state<0) return false ;

        Appointment& localappt = cal.getAppointment(i) ;
        QDateTime from = localappt.getDate(Appointment::From) ;
        QDateTime to = localappt.getDate(Appointment::To) ;

        // Just process entries within sync window
        if ( (from <= fromwindow && to >= fromwindow) ||
             (from >= fromwindow && to <= towindow) ||
             ( from <= towindow && to >= towindow) ) {

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
                        status = google.updateAppointment(localappt, GoogleAccess::Post) ;
                    } else {
                        status = google.updateAppointment(localappt, GoogleAccess::Put) ;
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

    if (networkok) {
        ui->updateStatusReport->append(QString("  Appointment Synchronisation: OK")) ;
    } else {
        ui->updateStatusReport->append(QString("  Appointment Synchronisation: FAILED - Please resync")) ;
    }

    ui->progressBar->setValue(500) ;
    return networkok ;
}
