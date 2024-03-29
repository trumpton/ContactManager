
#include "mainwindow.h"
#include "../Lib/alertsound.h"

//
// Calendar Tab
//

void MainWindow::LoadCalendarTab()
{
    QString id = calendar.getSelected().getField(Appointment::ID) ;
    int matchid=-1 ;

    // Refresh birthdays (all but mine), and sort ready for load
    calendar.purgeBirthdays();
    if (gConf->calendarShowBirthdays()) {
        for (int i=0; i<db.size(); i++) {
            Contact &c = db.getContact(i) ;
            if (c.getField(Contact::ID).compare(gConf->getMe())!=0) {
                QString when = c.getField(Contact::Birthday) ;
                QString who = QString("%1 %2").arg(c.getField(Contact::Names),c.getField(Contact::Surname)) ;
                QString whoid = c.getField(Contact::ID) ;
                calendar.addBirthday(who, whoid, when) ;
            }
        }
    }

    calendar.sort() ;
    calendarlist.clear() ;
    int ne=calendar.size() ;
    for (int i=0,j=0; i<ne; i++) {
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
              matchid = j ;
          }
          j++ ;
       }
    }

    calendarlist.appendData(" -- Add New Appointment for Me (Control-Shift-N) -- ", "Add New Appointment for Me", "ADD") ;
    if (matchid<0) {
        matchid=calendarlist.rowCount() - 1 ;
        dbg(QString("listCalendar->setCurrentIndex(endoflist))")) ;
    } else {
        dbg(QString("listCalendar->setCurrentIndex(%1)").arg(matchid)) ;
    }
    ui->listCalendar->setCurrentIndex(calendarlist.index(matchid,0)) ;
}


void MainWindow::SaveCalendarTab()
{
    int idx = ui->listCalendar->currentIndex().row() ;
    QString id = calendarlist. hintAt(idx) ;
    calendar.selectAppointment(id) ;
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
    if (appt.isTemp() || appt.isSet(Appointment::InternetOwned)) {
        // Error, read-only
        play(Error) ;
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

            qint64 duration = appt.getDate(Appointment::To).toSecsSinceEpoch() - appt.getDate(Appointment::From).toSecsSinceEpoch() ;

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
    calendar.selectAppointment(id) ;

    if (id.compare("ADD")==0) {

        on_actionNewMyAppointment_triggered() ;

    } else {

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

void MainWindow::on_listCalendar_SelectionChanged(const QModelIndex &index)
{
    dbg(QString("on_listCalendar_SelectionChanged(%1)").arg(index.row())) ;
    calendar.selectAppointment(calendarlist.data(index, Qt::UserRole).toString()) ;
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
    QString me = gConf->getMe() ;

    if (dr.isNull()) {

        play(Disabled) ;

    } else if (dr.getField(Contact::ID).compare(me)==0) {

        on_actionNewMyAppointment_triggered();

    } else {

        if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
            dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
            ui->tabBar->setCurrentIndex(CALENDARTABPOS);
        }

      dbg("listCalendar->setFocus()") ;
      ui->listCalendar->setFocus();

      Appointment reference = calendar.getNull() ;
      Appointment newappt ;
      newappt.setField(Appointment::Summary,QString("Appointment"));
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
        newappt.setField(Appointment::Summary,"Event");
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
// Buttons on Calendar Dialog
//

/*
void MainWindow::on_newAppointmentButton_clicked()
{
    on_actionNewAppointment_triggered() ;
}

void MainWindow::on_insertAppointmentButton_clicked()
{
    on_action_Insert_Appointment_After_triggered();
}
*/

void MainWindow::on_actionMoveAppointment_triggered()
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

void MainWindow::on_actionRemoveAppointment_triggered()
{
    int idx = ui->listCalendar->currentIndex().row() ;
    QString id = calendarlist.hintAt(idx) ;

    Appointment& appt = calendar.getAppointmentBy(Appointment::ID, id) ;
    if (appt.isNull() || appt.isTemp()) {
        play(Disabled) ;
    } else {
        if (warningYesNoDialog(this, "Delete Appointment?", "Are you sure you want to delete " +
                                 appt.asText() +
                                 ".\nThis is not easily undone....")) {

            QString nextid = calendarlist.hintAt(idx+1) ;
            appt.setFlag(Appointment::Deleted, true) ;
            calendar.save() ;
            LoadTabs() ;

            QModelIndex index = calendarlist.find(nextid) ;

            if (!index.isValid()) {
                // No entry found, goto end of list
                int rows = calendarlist.rowCount() ;
                index = calendarlist.index(rows-1,0) ;
            }

            if (index.isValid()) {
                dbg("listCalendar->setCurrentIndex()") ;
                ui->listCalendar->setCurrentIndex(index) ;
            }

        }
    }
}
