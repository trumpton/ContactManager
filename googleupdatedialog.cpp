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

    // Set the OAUTH2 Refresh Token
    QString refreshtoken = gConf->getGoogleOauth2RefreshToken() ;
    google.setupRToken(refreshtoken) ;

    // Set the sync tokens before synchronisation
    google.setCalendarSyncToken(gConf->getLastGoogleCalendarSyncToken());
    google.setContactSyncToken(gConf->getLastGoogleContactSyncToken());

    QString contactsyncdatestr = gConf->getLastGoogleContactSyncDate() ;
    QDateTime contactsyncdate = isoStringToDateTime(contactsyncdatestr) ;
    QString calendarsyncdatestr = gConf->getLastGoogleCalendarSyncDate() ;
    QDateTime calendarsyncdate = isoStringToDateTime(calendarsyncdatestr) ;

    bool calsuccess = true ;

    ui->updateStatusReport->append(QString("\nSYNCHRONISING CALENDAR")) ;
    ui->updateStatusReport->append(QString("Last Calendar Synchronisation Date: ") + calendarsyncdatestr ) ;

    calsuccess = processCalendarUpdate(calendarsyncdate, google, cal) ;
    if (!calsuccess) {
        ui->updateStatusReport->append(QString("CALENDAR SYNCHRONISATION FAILED: ") + google.getNetworkError()) ;
    } else {

        // Sleep to allow local changes that have been made during the sync to be
        // recorded as before the sync, to prevent re-syncing
        qSleep(1000) ;
        QString nextsyncdate = nowToIsoString() ;
        gConf->setLastGoogleCalendarSyncDate(nextsyncdate) ;

        // And store sync token for next time
        gConf->setLastGoogleCalendarSyncToken(google.getCalendarSyncToken());

        ui->updateStatusReport->append(QString("CALENDAR SYCHRONISATION SUCCESS.")) ;
    }

    ui->progressBar->setValue(500) ;

    ui->updateStatusReport->append(QString("\nSYNCHRONISING CONTACTS")) ;
    ui->updateStatusReport->append(QString("Last Contact Synchronisation Date: ") + contactsyncdatestr ) ;
    bool consuccess = processContactUpdate(contactsyncdate, google, db) ;
    if (!consuccess) {
        ui->updateStatusReport->append(QString("CONTACTS SYNCHRONISATION FAILED: ") + google.getNetworkError()) ;
    } else {
        // next syncdate after sync means anything changed on google during the sync may be lost
        qSleep(1000) ;
        QString nextsyncdate = nowToIsoString() ;
        gConf->setLastGoogleContactSyncDate(nextsyncdate) ;

        // And store sync token for next time
        gConf->setLastGoogleContactSyncToken(google.getContactSyncToken());

        ui->updateStatusReport->append(QString("CONTACTS SYCHRONISATION SUCCESS.")) ;
    }

    ui->updateStatusReport->append(QString("")) ;
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
