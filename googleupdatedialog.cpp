#include "googleupdatedialog.h"
#include "ui_googleupdatedialog.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"
#include "../Lib/alertsound.h"
#include "contactdatabase.h"
#include "calendar.h"
#include "contact.h"


GoogleUpdateDialog::GoogleUpdateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GoogleUpdateDialog)
{
    state = 0 ;
    ui->setupUi(this);
}

GoogleUpdateDialog::~GoogleUpdateDialog()
{
    delete ui;
}


int GoogleUpdateDialog::doUpdate(GoogleAccess& google, class ContactDatabase& db, class Calendar& cal)
{
    state = 1 ; // Processing

    ui->progressBar->setRange(0, 1000);
    ui->progressBar->setValue(0) ;

    setModal(true) ;

    ui->pushButton->setAccessibleName("Abort Update") ;
    ui->pushButton->setText("Abort Update");

    show() ;

    ui->pushButton->setFocus() ;

    // Start of the sync.  Birthdays come from the contacts list, so the stored copies are
    // moved to the deleted folder.  A blank contact never uploaded has nothing on Google, so
    // it is marked deleted now and moved at the end.  Blank contacts already on Google are
    // handled in the contact sync, once Google's copy has been downloaded.
    int birthdays = cal.tidyBirthdays(false) ;
    ui->updateStatusReport->append(QString("Moved %1 stored birthday entries to the deleted folder.").arg(birthdays)) ;

    Contact& selectedcontact = db.getSelected() ;
    int blanks = 0 ;
    for (int i=0; i<db.size(); i++) {
        Contact& localcontact = db.getContact(i) ;
        if (&localcontact==&selectedcontact || localcontact.isSet(Contact::Hidden) || localcontact.isSet(Contact::Deleted)) continue ;
        if (!localcontact.getField(Contact::GoogleRecordId).isEmpty() || !localcontact.isBlank()) continue ;
        localcontact.setFlag(Contact::Deleted, true) ;
        blanks++ ;
    }
    ui->updateStatusReport->append(QString("Marked %1 empty contact(s) for removal.").arg(blanks)) ;

    // Set the OAUTH2 Refresh Token
    QString refreshtoken = gConf->getGoogleOauth2RefreshToken() ;
    google.setupRToken(refreshtoken) ;

    // Set the sync tokens before synchronisation
//    google.setCalendarSyncToken(gConf->getLastGoogleCalendarSyncToken());
//    google.setContactSyncToken(gConf->getLastGoogleContactSyncToken());

    QString contactsyncdatestr = gConf->getLastGoogleContactSyncDate() ;
    QDateTime contactsyncdate = isoStringToDateTime(contactsyncdatestr) ;
    QString calendarsyncdatestr = gConf->getLastGoogleCalendarSyncDate() ;
    QDateTime calendarsyncdate = isoStringToDateTime(calendarsyncdatestr) ;

    bool calsuccess = true ;

    ui->updateStatusReport->append(QString("\nSYNCHRONISING CALENDAR")) ;

    if (gConf->calendarSyncEnabled()) {

        ui->updateStatusReport->append(QString("\nSYNCHRONISING CALENDAR")) ;
        ui->updateStatusReport->append(QString("Last Calendar Synchronisation Date: ") + calendarsyncdatestr ) ;

        calsuccess = processCalendarUpdate(calendarsyncdate, google, db, cal) ;
        if (!calsuccess) {
            ui->updateStatusReport->append(QString("CALENDAR SYNCHRONISATION FAILED: ") + google.getNetworkError()) ;
        } else {

            // Sleep to allow local changes that have been made during the sync to be
            // recorded as before the sync, to prevent re-syncing
            qSleep(1000) ;
            QString nextsyncdate = nowToIsoString() ;
            gConf->setLastGoogleCalendarSyncDate(nextsyncdate) ;

            // And store sync token for next time
//            gConf->setLastGoogleCalendarSyncToken(google.getCalendarSyncToken());

            ui->updateStatusReport->append(QString("CALENDAR SYCHRONISATION SUCCESS.")) ;
        }

    } else {

        ui->updateStatusReport->append(QString("Calendar Synchronisation disabled in configuration.")) ;

    }

    ui->progressBar->setValue(500) ;

    bool consuccess = true ;

    ui->updateStatusReport->append(QString("\nSYNCHRONISING CONTACTS")) ;

    if (gConf->contactSyncEnabled()) {

        ui->updateStatusReport->append(QString("Last Contact Synchronisation Date: ") + contactsyncdatestr ) ;
        consuccess = processContactUpdate(contactsyncdate, google, db) ;
        if (!consuccess) {
            ui->updateStatusReport->append(QString("CONTACTS SYNCHRONISATION FAILED: ") + google.getNetworkError()) ;
        } else {
            // next syncdate after sync means anything changed on google during the sync may be lost
            qSleep(1000) ;
            QString nextsyncdate = nowToIsoString() ;
            gConf->setLastGoogleContactSyncDate(nextsyncdate) ;

            // And store sync token for next time
//            gConf->setLastGoogleContactSyncToken(google.getContactSyncToken());

            ui->updateStatusReport->append(QString("CONTACTS SYCHRONISATION SUCCESS.")) ;
        }

    } else {

        ui->updateStatusReport->append(QString("Calendar Synchronisation disabled in configuration.")) ;

    }

    ui->updateStatusReport->append(QString("")) ;
    // Last step: move deleted contacts with no Google link to the deleted folder.  A deleted
    // contact still linked to Google has not yet been confirmed as removed there, so it stays.
    if (state>=0) {
        int linked=0 ;
        int archived = db.tidyDeleted(linked) ;
        ui->updateStatusReport->append(QString("Moved %1 deleted contact(s) to the deleted folder.").arg(archived)) ;
        if (linked>0) {
            ui->updateStatusReport->append(QString("%1 deleted contact(s) still have a Google link; run Google Sync again.").arg(linked)) ;
        }
    }

    ui->progressBar->setValue(1000) ;
    play(Query) ;

    if (state<0) {

        // Abort Pressed

    } else {

        // Update button labels and wait for press
        if (consuccess  && calsuccess) {
            ui->pushButton->setAccessibleName("Update Complete") ;
            ui->pushButton->setText("Update Complete");
            ui->pushButton->setFocus() ;
            state = 2 ;
        } else {
            ui->pushButton->setAccessibleName("Update Failed") ;
            ui->pushButton->setText("Update Failed");
            ui->pushButton->setFocus() ;
            state = 3 ;
        }
        exec() ;

    }

    hide() ;
    return state ;
}

// Abort Button
void GoogleUpdateDialog::on_pushButton_clicked()
{
    switch (state) {
    case 0:     // Not Running
        state=0 ;
        break ;
    case 1:     // Running, Abort Pressed
        state=-1 ;
        break ;
    case 2:     // Finished (Success), OK Pressed
        accept() ;
        break ;
    case 3:     // Finished (Failure), OK Pressed
        accept() ;
        break ;
    }
}
