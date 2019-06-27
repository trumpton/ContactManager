
#include "mainwindow.h"
#include "../Lib/alertsound.h"

//
// Calendar Tab
//

void MainWindow::LoadCalendarTab()
{
    int idx = ui->listCalendar->currentIndex().row() ;
    QString id = calendarlist. hintAt(idx) ;
    int matchidx=0 ;

    calendarlist.clear() ;
    calendar.sort() ;
    int ne=calendar.size() ;
    for (int i=0; i<ne; i++) {
       Appointment &appt = calendar.getAppointment(i) ;
       if (!appt.isSet(Appointment::Deleted) &&
               appt.isInRange(APPOINTMENTSTARTWINDOW, APPOINTMENTENDWINDOW) ) {
           QString apptfor ;
           QString apptforid = appt.getField(Appointment::ContactId) ;
           if (!apptforid.isEmpty() && appt.isNotFor(gConf->getMe())) {
               Contact& contact = db.getContactById(apptforid) ;
               apptfor = contact.getFormattedName(false, false) ;
           }
           QString title = appt.asText(apptfor) ;
           QString accessibletitle = appt.asAccessibleText(apptfor) ;
           calendarlist.appendData(title, accessibletitle, appt.getField(Appointment::ID)) ;
          if (id.compare(appt.getField(Appointment::ID))==0) {
              matchidx = i ;
          }
       }
    }

    // Restore Calendar Index
    dbg("listCalendar->setCurrentIndex()") ;
    ui->listCalendar->setCurrentIndex(calendarlist.index(matchidx,0)) ;
}


void MainWindow::SaveCalendarTab()
{
}

//
// POPUP FORMS
//

void MainWindow::editAppointment(Appointment &editing, Appointment &reference, QString contactid, bool createnew)
{
    class AppointmentForm *frm ;
    class Appointment appt, clash ;
    bool retry = false ;
    qint64 ret ;
    QString oldfrom ;

    // TODO: appt needs a copy of the save path
    // not sure of the best way to achieve this
    appt = editing ;

    // TODO TODO TODO TODO
    // Check appointment is editable
    if (appt.isSet(Appointment::InternetOwned)) {
        // Error, read-only
        return ;
    }

    oldfrom = appt.getField(Appointment::From) ;
    if (contactid.isEmpty()) contactid = editing.getContactId() ;

    frm = new AppointmentForm(this) ;
    frm->setupForm(db, contactid, editing, reference, createnew) ;

    do {

        play(Query) ;
        ret=frm->exec() ;
        appt = frm->getAppointmentDetails() ;

        if (ret==0) {

            retry = false ;

        } else {

            clash = calendar.getAppointmentClash(appt) ;

            qint64 duration = appt.getDate(Appointment::To).toTime_t() - appt.getDate(Appointment::From).toTime_t() ;

            if (!clash.isNull()) {
                QString msg ;
                 msg = "You have an appointment clash, between\n" ;
                msg = msg + appt.asText(db.getContactById(appt.getField(Appointment::ContactId)).getFormattedName(false, false)) + ", and\n" ;
                msg = msg + clash.asText(db.getContactById(clash.getField(Appointment::ContactId)).getFormattedName(false, false)) + "\n" ;
                msg = msg + "Select Yes/Enter to save anyway, or No/Escape to go back and change it" ;
                retry = !warningYesNoDialog(this, "Appointment Clash", msg) ;
            } else if (!appt.isInRange(APPOINTMENTSTARTWINDOW, APPOINTMENTENDWINDOW)) {
                warningOkDialog(this, "Invalid Date", "You have set an invalid date for the appointment, please fix it") ;
                retry = true ;
            } else if (duration==0) {
                retry = !warningYesNoDialog(this, "Appointment Check", "Your appointment is 0 minutes long, do you want that set as a reminder?") ;
            } else if (duration>(24*3600)) {
                retry = !warningYesNoDialog(this, "Appointment Check", "Your appointment is longer than a day, is that right?") ;
            }
        }
    } while (retry) ;

    if (ret!=0) {
        calendar.addAppointment(appt) ;
        calendar.sort() ;
        LoadTabs() ;

        QString thisid = appt.getField(Appointment::ID) ;
        QModelIndex index = calendarlist.find(thisid) ;

        dbg("listCalendar->setCurrentIndex()") ;
        ui->listCalendar->setCurrentIndex(index) ;

    }

    calendar.save() ;
    delete frm ;

}

//
//
//

// Change contact to match the selected one
void MainWindow::on_listCalendar_activated(const QModelIndex &index)
{
    Q_UNUSED(index) ;
    int idx = ui->listCalendar->currentIndex().row() ;
    QString id = calendarlist.hintAt(idx) ;
    QString contactid = calendar.getAppointmentBy(Appointment::ID, id).getField(Appointment::ContactId) ;
    if (contactid.isEmpty()) {
        play(NotFound) ;
    }
    if (!contactid.isEmpty()) {
        populateDialog(contactid) ;

        if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
            dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
            ui->tabBar->setCurrentIndex((CALENDARTABPOS));
        }

        dbg("listCalendar->setFocus()") ;
        ui->listCalendar->setFocus();
    }
}

//
// single click on an entry - update debug information
// if debug is enabled.
//
void MainWindow::on_listCalendar_clicked(const QModelIndex &index)
{
    Q_UNUSED(index) ;
    if (gConf->debugGoogleEnabled()) {
        // TODO: if debug
        int idx = ui->listCalendar->currentIndex().row() ;
        QString id = calendarlist.hintAt(idx) ;
        msg(id) ;
    }
}


//
// Appointment Menu Options
//

//
// New Appointment for Current Contact
//

void MainWindow::on_actionNewAppointment_triggered()
{
    Contact &dr = db.getSelected() ;
    if (dr.isNull()) {
        play(Disabled) ;
    } else {

        if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
            dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
            ui->tabBar->setCurrentIndex(CALENDARTABPOS);
        }

      dbg("listCalendar->setFocus()") ;
      ui->listCalendar->setFocus();

      Appointment reference = calendar.getNull() ;
      Appointment newappt ;
      editAppointment(newappt, reference, db.getSelected().getField(Contact::ID), true) ;
   }
}

//
// New Appointment for Me
//

void MainWindow::on_actionNewMyAppointment_triggered()
{
    QString me = gConf->getMe() ;
    if (me.isEmpty()) {
        errorOkDialog(this, "Error, Not Configured", "Me! not configured, so I can't find my list") ;
        return ;
    } else {
        Appointment reference = calendar.getNull() ;
        Appointment newappt ;
        editAppointment(newappt, reference, me, true) ;
    }
}

//
// Insert Appointment After Selection
//

void MainWindow::on_action_Insert_Appointment_After_triggered()
{
    Contact &dr = db.getSelected() ;
    if (dr.isNull()) {
        play(Disabled) ;
    } else {

        if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
            dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
            ui->tabBar->setCurrentIndex(CALENDARTABPOS);
        }

        dbg("listCalendar->setFocus()") ;
        ui->listCalendar->setFocus();

        Appointment reference ;
        int idx = ui->listCalendar->currentIndex().row() ;

        if (idx>=0) {
            QString id = calendarlist.hintAt(idx) ;
            reference = calendar.getAppointmentBy(Appointment::ID, id) ;
            Appointment newappt ;
            editAppointment(newappt, reference, db.getSelected().getField(Contact::ID), true) ;
        } else {
            play(NotFound) ;
        }
    }
}


//
//  Insert My Appointment After Selection
//
//

void MainWindow::on_actionInsert_My_Appointment_After_triggered()
{
    QString me = gConf->getMe() ;
    if (me.isEmpty()) {
        errorOkDialog(this, "Error, Not Configured", "Me! not configured, so I can't find my list") ;
        return ;
    } else {
        int idx = ui->listCalendar->currentIndex().row() ;
        QString id = calendarlist.hintAt(idx) ;
        if (idx<0) {
            play(NotFound) ;
        } else {
            Appointment reference = calendar.getAppointmentBy(Appointment::ID, id) ;
            Appointment newappt ;
            editAppointment(newappt, reference, me, true) ;
        }
    }
}


//
// Move Appointments
//

void MainWindow::on_moveAppointmentButton_clicked()
{
    // TODO: If calendar not selected, beep
    int idx = ui->listCalendar->currentIndex().row() ;
    QString id = calendarlist.hintAt(idx) ;
    Appointment& appt = calendar.getAppointmentBy(Appointment::ID, id) ;
    if (appt.isNull()) {
        play(NotFound) ;
    } else {
// TODO: if contactid is empty, editAppointment, and enable the contact selection
// If contact changed, set created date to now
        editAppointment(appt, appt) ;
    }
}

//
// Remove Appointment
//

void MainWindow::on_removeAppointmentButton_clicked()
{
    int idx = ui->listCalendar->currentIndex().row() ;
    QString id = calendarlist.hintAt(idx) ;
    Appointment& appt = calendar.getAppointmentBy(Appointment::ID, id) ;
    if (appt.isNull()) {
        play(NotFound) ;
    } else {
        if (warningYesNoDialog(this, "Delete Appointment?", "Are you sure you want to delete " +
                                 appt.asText() +
                                 ".\nThis is not easily undone....")) {

            appt.setFlag(Appointment::Deleted, true) ;
            calendar.save() ;
            LoadTabs() ;
            on_actionGotoToday_triggered() ;
        }
    }
}

//
// Buttons on Calendar Dialog
//


void MainWindow::on_newAppointmentButton_clicked()
{
    on_actionNewAppointment_triggered() ;
}

void MainWindow::on_insertAppointmentButton_clicked()
{
    on_action_Insert_Appointment_After_triggered();
}

void MainWindow::on_actionMoveAppointment_triggered()
{
    on_moveAppointmentButton_clicked() ;
}

void MainWindow::on_actionRemoveAppointment_triggered()
{
     on_removeAppointmentButton_clicked() ;
}

