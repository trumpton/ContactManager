
#include <algorithm>
#include "mainwindow.h"
#include "../Lib/alertsound.h"

//
// Calendar Tab
//

void MainWindow::LoadCalendarTab()
{
    // The entry on screen: a repeat has its own hint, and an appointment has its ID
    QString selected = calendar.selectedHint().isEmpty() ? calendar.getSelected().getField(Appointment::ID) : calendar.selectedHint() ;
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

    // A row for an entry, shown at a start.  A repeat is shown at its own start and end, and its hint is its own.
    struct Row { QDateTime start ; QDateTime end ; QString title ; QString accessible ; QString hint ; } ;
    QList<Row> rows ;
    auto addRow = [&](Appointment &appt, QDateTime start, QString hint) {
        QDateTime from = appt.getDate(Appointment::From) ;          // getDate shares one value, so copy each date
        QDateTime to = appt.getDate(Appointment::To) ;
        Appointment shown = appt ;
        shown.setDate(Appointment::From, start) ;
        shown.setDate(Appointment::To, start.addSecs(from.secsTo(to))) ;
        QString apptfor ;
        QString apptforid = appt.getField(Appointment::ContactId) ;
        if (!apptforid.isEmpty() && appt.isNotFor(gConf->getMe())) {
            Contact& contact = db.getContactById(apptforid) ;
            apptfor = contact.getFormattedName(false, false) ;
        }
        Row row ;
        row.start = start ;
        row.end = start.addSecs(from.secsTo(to)) ;
        row.title = shown.asText(apptfor) ;
        row.accessible = shown.asAccessibleText(apptfor) ;
        row.hint = hint ;
        rows << row ;
    } ;

    // The appointment window, from the settings.  An appointment is listed if it overlaps the window, and a repeat of one
    // is listed if its occurrence does.  A repeat is worked out here from its appointment, and is not stored.  The
    // appointment is the first occurrence, so it is not listed again as a repeat.  Its source may be outside the window,
    // in which case only its repeats in the window are listed.
    int windowbefore = gConf->calendarWindowBefore() ;
    int windowafter = gConf->calendarWindowAfter() ;
    QDateTime now = QDateTime::currentDateTimeUtc() ;
    QDateTime windowstart = now.addDays(-windowbefore) ;
    QDateTime windowend = now.addDays(windowafter) ;
    const int maxrepeats = 100000 ;
    int ne=calendar.size() ;
    for (int i=0; i<ne; i++) {
       Appointment &appt = calendar.getAppointment(i) ;
       if (appt.isSet(Appointment::Deleted)) continue ;
       QString apptid = appt.getField(Appointment::ID) ;
       if (appt.isInRange(windowbefore, windowafter)) {
           addRow(appt, appt.getDate(Appointment::From), apptid) ;
       }
       if (appt.isRepeating()) {
           QDateTime apptfrom = appt.getDate(Appointment::From) ;     // copies: getDate shares one value
           QDateTime apptto = appt.getDate(Appointment::To) ;
           qint64 length = apptfrom.secsTo(apptto) ;
           for (int n=1; n<maxrepeats; n++) {
               QDateTime start = appt.occurrenceFrom(n) ;
               if (start > windowend) break ;
               if (!appt.occurrenceExists(n)) break ;
               if (start.addSecs(length) > windowstart) addRow(appt, start, Calendar::occurrenceHint(apptid, start)) ;
           }
       }
    }

    // Start order, with the entries that start together kept in the order they were found
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.start < b.start ; }) ;

    // The entry on screen is kept.  If it is no longer listed, its appointment is, so the list is not left at the end.
    calendarrowends.clear() ;
    for (int j=0; j<rows.size(); j++) {
        calendarlist.appendData(rows[j].title, rows[j].accessible, rows[j].hint) ;
        calendarrowends << rows[j].end ;
        if (matchid<0 && rows[j].hint.compare(selected)==0) matchid = j ;
    }
    // The entry on screen is no longer listed, as when a repeat is shortened.  The entry of the same appointment that is
    // nearest to it takes the focus, so the focus stays where it was.
    if (matchid<0) {
        QString selectedid = Calendar::appointmentIdOf(selected) ;
        QDateTime wanted = QDateTime::fromString(selected.section('@', 1), Qt::ISODate) ;
        qint64 nearest = -1 ;
        for (int j=0; j<rows.size(); j++) {
            if (Calendar::appointmentIdOf(rows[j].hint).compare(selectedid)!=0) continue ;
            qint64 gap = wanted.isValid() ? qAbs(rows[j].start.secsTo(wanted)) : 0 ;
            if (nearest<0 || gap<nearest) {
                nearest = gap ;
                matchid = j ;
            }
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

// The appointment that an edit of one of its repeats changes.  The form shows the repeat's own day, so when that day is
// moved, the appointment moves by the same amount.  The form's fields, and the duration it sets, are kept.
static Appointment appointmentForRepeat(Appointment &result, Appointment &original, QDateTime repeatstart)
{
    QDateTime formfrom = result.getDate(Appointment::From) ;     // getDate shares one value, so copy each date
    QDateTime formto = result.getDate(Appointment::To) ;
    QDateTime origfrom = original.getDate(Appointment::From) ;
    qint64 shift = repeatstart.secsTo(formfrom) ;

    Appointment moved = result ;
    QDateTime newfrom = origfrom.addSecs(shift) ;
    moved.setDate(Appointment::From, newfrom) ;
    moved.setDate(Appointment::To, newfrom.addSecs(formfrom.secsTo(formto))) ;

    // A repeat's end moves with the appointment
    QString until = result.getField(Appointment::RepeatUntil) ;
    if (!until.isEmpty() && shift!=0) {
        moved.setField(Appointment::RepeatUntil, QDateTime::fromString(until, Qt::ISODate).addSecs(shift).toUTC().toString(Qt::ISODate)) ;
    }
    return moved ;
}

// Saves an edit made in the form.  An edit of a repeat changes its appointment, so every repeat changes.  The focus stays
// on the repeat, unless it was moved to another day, when it goes to the appointment.
void MainWindow::storeAppointmentEdit(Appointment &result, QString occurrence)
{
    QString originid = result.getField(Appointment::ID) ;
    bool repeat = !occurrence.isEmpty() ;
    QDateTime repeatstart = QDateTime::fromString(occurrence, Qt::ISODate) ;

    Appointment stored = result ;
    if (repeat) {
        Appointment original = calendar.getAppointmentBy(Appointment::ID, originid) ;
        stored = appointmentForRepeat(result, original, repeatstart) ;
    }
    calendar.addAppointment(stored) ;
    calendar.sort() ;

    QString hint = originid ;
    if (repeat) {
        bool daymoved = result.getDate(Appointment::From).toLocalTime().date() != repeatstart.toLocalTime().date() ;
        QDateTime newstart = result.getDate(Appointment::From) ;
        QDateTime originstart = stored.getDate(Appointment::From) ;
        if (!daymoved) {
            hint = (newstart == originstart) ? originid : Calendar::occurrenceHint(originid, newstart) ;
        }
    }
    calendar.selectAppointment(hint) ;
    LoadTabs() ;

    // LoadTabs has already chosen the row, including the nearest one when the edited entry is no longer listed.  An
    // invalid row is never set, as it leaves the list with no focus.
    QModelIndex index = calendarlist.find(hint) ;
    if (index.isValid()) {
        dbg("listCalendar->setCurrentIndex()") ;
        ui->listCalendar->setCurrentIndex(index) ;
    }
}

void MainWindow::editAppointment(Appointment &editing, Appointment &reference, QString contactid, bool createnew, QString occurrence)
{
    class AppointmentForm *frm ;
    class Appointment appt, clash ;
    bool retry = false ;
    qint64 ret ;
    QString oldfrom ;

    // A repeat is edited with the form showing its own day.  Its appointment is changed when it is saved.
    bool repeat = !occurrence.isEmpty() ;
    QDateTime repeatstart = QDateTime::fromString(occurrence, Qt::ISODate) ;
    Appointment shown = editing ;
    if (repeat) {
        QDateTime from = editing.getDate(Appointment::From) ;
        QDateTime to = editing.getDate(Appointment::To) ;
        shown.setDate(Appointment::From, repeatstart) ;
        shown.setDate(Appointment::To, repeatstart.addSecs(from.secsTo(to))) ;
    }

    // TODO: appt needs a copy of the save path
    // not sure of the best way to achieve this
    appt = shown ;

    // TODO TODO TODO TODO
    // Check appointment is editable
    if (appt.isTemp() || appt.isSet(Appointment::InternetOwned)) {
        // Error, read-only
        play(Error) ;
        return ;
    }

    oldfrom = appt.getField(Appointment::From) ;
    if (contactid.isEmpty()) contactid = editing.getContactId() ;

    do {

        // A new form for each attempt, showing the values of the last one.  A form that has been accepted does not close
        // on OK when it is shown again, so it is not reused.
        frm = new AppointmentForm(this) ;
        frm->setupForm(db, contactid, appt, reference, createnew) ;

        play(Query) ;
        ret=frm->exec() ;
        appt = frm->getAppointmentDetails() ;
        delete frm ;
        frm = NULL ;

        // The appointment as it would be saved, so the checks are made on that
        Appointment checked = repeat ? appointmentForRepeat(appt, editing, repeatstart) : appt ;

        if (ret==0) {

            retry = false ;

        } else {

            clash = calendar.getAppointmentClash(checked) ;

            qint64 duration = checked.getDate(Appointment::To).toSecsSinceEpoch() - checked.getDate(Appointment::From).toSecsSinceEpoch() ;

            if (!clash.isNull()) {
                QString msg ;
                 msg = "You have an appointment clash, between\n" ;
                msg = msg + checked.asText(db.getContactById(checked.getField(Appointment::ContactId)).getFormattedName(false, false)) + ", and\n" ;
                msg = msg + clash.asText(db.getContactById(clash.getField(Appointment::ContactId)).getFormattedName(false, false)) + "\n" ;
                msg = msg + "Select Yes/Enter to save anyway, or No/Escape to go back and change it" ;
                retry = !warningYesNoDialog(this, "Appointment Clash", msg) ;
            } else if (checked.getDate(Appointment::From) > QDateTime::currentDateTimeUtc().addDays(gConf->calendarWindowAfter())) {
                // An appointment can start before the window, as it can be set again after a deletion; it cannot start after it
                warningOkDialog(this, "Invalid Date", QString("The appointment starts more than %1 days ahead, which is after the end of the appointment window, so it cannot be shown.  Please fix the date.")
                                .arg(gConf->calendarWindowAfter())) ;
                retry = true ;
            } else if (duration==0) {
                retry = !warningYesNoDialog(this, "Appointment Check", "Your appointment is 0 minutes long, do you want that set as a reminder?") ;
            } else if (duration>(24*3600)) {
                retry = !warningYesNoDialog(this, "Appointment Check", "Your appointment is longer than a day, is that right?") ;
            }
        }
    } while (retry) ;

    if (ret!=0) {
        storeAppointmentEdit(appt, occurrence) ;
    }

    calendar.save() ;

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

        QString contactid = calendar.getAppointmentBy(Appointment::ID, Calendar::appointmentIdOf(id)).getField(Appointment::ContactId) ;
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
            reference = calendar.getAppointmentBy(Appointment::ID, Calendar::appointmentIdOf(id)) ;
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
            Appointment reference = calendar.getAppointmentBy(Appointment::ID, Calendar::appointmentIdOf(id)) ;
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
    Appointment& appt = calendar.getAppointmentBy(Appointment::ID, Calendar::appointmentIdOf(id)) ;
    if (appt.isNull()) {
        play(NotFound) ;
    } else {
// TODO: if contactid is empty, editAppointment, and enable the contact selection
// If contact changed, set created date to now
        // A repeat is edited with its own day shown; the hint carries that day after the @
        QString occurrence = (id.compare(Calendar::appointmentIdOf(id))==0) ? QString() : id.section('@', 1) ;
        editAppointment(appt, appt, QString(), false, occurrence) ;
    }
}

void MainWindow::on_actionRemoveAppointment_triggered()
{
    int idx = ui->listCalendar->currentIndex().row() ;
    QString id = calendarlist.hintAt(idx) ;

    Appointment& appt = calendar.getAppointmentBy(Appointment::ID, Calendar::appointmentIdOf(id)) ;
    if (appt.isNull() || appt.isTemp()) {
        play(Disabled) ;
    } else {
        // A repeat is its appointment, so deleting it deletes every repeat
        QString repeats = appt.isRepeating() ? QString(" and all its repeats") : QString() ;
        if (warningYesNoDialog(this, "Delete Appointment?", "Are you sure you want to delete " +
                                 appt.asText() + repeats +
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
