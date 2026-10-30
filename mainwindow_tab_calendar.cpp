
#include <algorithm>
#include "mainwindow.h"
#include "../Lib/warningchoice.h"
#include <QTimeZone>
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

            // An appointment for a deleted contact, or one whose contact has been removed, is still listed, so that it is not
            // lost.  Hidden contacts keep their names.
            if (contact.isNull() || contact.isSet(Contact::Deleted)) apptfor = "Deleted contact" ;
            else apptfor = contact.getFormattedName(false, false) ;
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
       // The first occurrence is not listed when the repeat leaves it out
       bool originlisted = !(appt.isRepeating() && appt.isException(appt.getDate(Appointment::From))) ;
       if (appt.isInRange(windowbefore, windowafter) && originlisted) {
           addRow(appt, appt.getDate(Appointment::From), apptid) ;
       }
       if (appt.isRepeating()) {
           QDateTime apptfrom = appt.getDate(Appointment::From) ;     // copies: getDate shares one value
           QDateTime apptto = appt.getDate(Appointment::To) ;
           qint64 length = apptfrom.secsTo(apptto) ;
           for (int n=1; n<maxrepeats; n++) {
               QDateTime start = appt.occurrenceFrom(n) ;
               if (start > windowend) break ;
               if (appt.occurrenceEnded(n)) break ;
               if (appt.isException(start)) continue ;
               if (start.addSecs(length) > windowstart) addRow(appt, start, Calendar::occurrenceHint(apptid, start)) ;
           }
       }
    }

    // Start order, with the entries that start together kept in the order they were found
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.start < b.start ; }) ;

    // The entry on screen is kept.  If it is no longer listed, its appointment is, so the list is not left at the end.
    calendarrowends.clear() ;
    calendarrowstarts.clear() ;
    for (int j=0; j<rows.size(); j++) {
        calendarlist.appendData(rows[j].title, rows[j].accessible, rows[j].hint) ;
        calendarrowends << rows[j].end ;
        calendarrowstarts << rows[j].start ;
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

    // The move is the difference between clock times in the repeat's zone, so a move across a clock change keeps the
    // clock time of the appointment.  The clock times are kept as plain wall-clock values (UTC, so no zone rules apply).
    QTimeZone zone = original.repeatTimeZone() ;
    auto clock = [zone](QDateTime when) {
        QDateTime local = when.toTimeZone(zone) ;
        return QDateTime(local.date(), local.time(), Qt::UTC) ;
    } ;
    qint64 shift = clock(repeatstart).secsTo(clock(formfrom)) ;

    Appointment moved = result ;
    QDateTime newfrom = Appointment::wallClockToUtc(clock(origfrom).addSecs(shift), zone) ;
    moved.setDate(Appointment::From, newfrom) ;
    moved.setDate(Appointment::To, newfrom.addSecs(formfrom.secsTo(formto))) ;

    // A repeat's end moves with the appointment
    QString until = result.getField(Appointment::RepeatUntil) ;
    if (!until.isEmpty() && shift!=0) {
        QDateTime newuntil = clock(QDateTime::fromString(until, Qt::ISODate)).addSecs(shift) ;
        moved.setField(Appointment::RepeatUntil, Appointment::wallClockToUtc(newuntil, zone).toString(Qt::ISODate)) ;
    }
    return moved ;
}

// The edit of one appointment of a repeat, as a choice of what changes.  Returns the index of the choice, or -1.
static int chooseRepeatChange(QWidget *parent, Appointment &result)
{
    WarningChoice choice(parent) ;
    choice.setText("Change which appointments of this repeat?\n" + result.asText()) ;
    choice.addChoice("Change just this one", 'a', "The other appointments in the repeat are not changed") ;
    choice.addChoice("Change this one and all following", 'b', "The appointments before it are not changed") ;
    choice.addChoice("Change all from today onwards", 'c', "The appointments before today are not changed") ;
    choice.addChoice("Cancel", 'd', "Nothing is changed") ;
    choice.setDefaultChoice(3) ;
    return choice.choose() ;
}

// The first occurrence on or after today, in the zone of the repeat, or -1 if there is none
static int indexFromToday(Appointment &original)
{
    QTimeZone zone = original.repeatTimeZone() ;
    QDate today = QDateTime::currentDateTimeUtc().toTimeZone(zone).date() ;
    for (int n=0; !original.occurrenceEnded(n); n++) {
        if (original.occurrenceFrom(n).toTimeZone(zone).date() >= today) return n ;
    }
    return -1 ;
}

// Just this one: the occurrence is left out of the repeat, and the edited appointment is a new one.  Returns its ID.
static QString changeJustThisOne(Calendar &cal, Appointment &original, Appointment &result, QDateTime repeatstart)
{
    original.addException(repeatstart) ;
    cal.addAppointment(original) ;

    Appointment single = result.copyAsNew() ;
    single.setField(Appointment::Repeat, "0") ;
    single.setField(Appointment::RepeatInterval, "") ;
    single.setField(Appointment::RepeatCount, "") ;
    single.setField(Appointment::RepeatUntil, "") ;
    single.setField(Appointment::RepeatZone, "") ;
    single.setField(Appointment::RepeatExceptions, "") ;
    cal.addAppointment(single) ;
    return single.getField(Appointment::ID) ;
}

// This one and all following, from occurrence number from (not the first): the repeat ends before it, and a new repeat
// starts there with the edit.  The occurrences it leaves out from there on move with the new repeat.  Returns its ID.
static QString changeThisAndFollowing(Calendar &cal, Appointment &original, Appointment &result, QDateTime repeatstart, int from)
{
    QTimeZone zone = original.repeatTimeZone() ;
    QDateTime boundary = original.occurrenceFrom(from) ;

    // The edit moves the occurrence by this many seconds of clock time, as in appointmentForRepeat
    auto clock = [zone](QDateTime when) {
        QDateTime local = when.toTimeZone(zone) ;
        return QDateTime(local.date(), local.time(), Qt::UTC) ;
    } ;
    qint64 shift = clock(repeatstart).secsTo(clock(result.getDate(Appointment::From))) ;

    Appointment base = original ;
    base.setDate(Appointment::From, boundary) ;
    Appointment rest = appointmentForRepeat(result, base, repeatstart).copyAsNew() ;
    int count = original.getField(Appointment::RepeatCount).toInt() ;
    if (count>0) rest.setField(Appointment::RepeatCount, QString::number(count-from)) ;

    QList<QDateTime> kept, moved ;
    foreach (const QDateTime &left, original.exceptions()) {
        if (left < boundary) kept.append(left) ;
        else moved.append(Appointment::wallClockToUtc(clock(left).addSecs(shift), zone)) ;
    }
    original.setExceptions(kept) ;
    original.endBefore(from) ;
    rest.setExceptions(moved) ;

    cal.addAppointment(original) ;
    cal.addAppointment(rest) ;
    return rest.getField(Appointment::ID) ;
}

// Saves an edit made in the form.  An edit of a repeat asks which of its appointments change: just this one, this one and
// all following, or all from today.  Cancel changes nothing.  The focus stays on the entry that was edited.
void MainWindow::storeAppointmentEdit(Appointment &result, QString occurrence)
{
    QString originid = result.getField(Appointment::ID) ;
    bool repeat = !occurrence.isEmpty() ;
    QDateTime repeatstart = QDateTime::fromString(occurrence, Qt::ISODate) ;

    Appointment stored = result ;
    QString hint = originid ;
    if (repeat) {
        Appointment original = calendar.getAppointmentBy(Appointment::ID, originid) ;
        int chosen = chooseRepeatChange(this, result) ;
        if (chosen<0 || chosen==3) return ;

        int n = original.indexOfOccurrence(repeatstart) ;
        if (n<0) return ;
        int from = (chosen==2) ? indexFromToday(original) : n ;
        if (from<0) {
            warningOkDialog(this, "Change Repeat", "There are no appointments from today onwards in this repeat.") ;
            return ;
        }

        if (chosen==0) {
            hint = changeJustThisOne(calendar, original, result, repeatstart) ;
        } else if (from>0) {
            hint = changeThisAndFollowing(calendar, original, result, repeatstart, from) ;
        } else {
            // The first appointment of the repeat: every appointment of the repeat changes, as before
            stored = appointmentForRepeat(result, original, repeatstart) ;
            calendar.addAppointment(stored) ;
            bool daymoved = result.getDate(Appointment::From).toLocalTime().date() != repeatstart.toLocalTime().date() ;
            QDateTime newstart = result.getDate(Appointment::From) ;
            QDateTime originstart = stored.getDate(Appointment::From) ;
            if (!daymoved) {
                hint = (newstart == originstart) ? originid : Calendar::occurrenceHint(originid, newstart) ;
            }
        }
    } else {
        calendar.addAppointment(stored) ;
    }
    calendar.sort() ;

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

            // With a contact open, moving to the contact of this appointment is confirmed first.  On No, the open contact
            // stays as it is.
            bool switchto = true ;
            Contact &open = db.getSelected() ;
            QString openid = open.getField(Contact::ID) ;
            bool contactopen = !open.isNull() && !openid.isEmpty() && openid.compare(SELECT_OVERVIEW)!=0 ;
            if (contactopen && openid.compare(contactid)!=0) {
                QString fromname = open.getFormattedName(false, false) ;
                QString toname = db.getContactById(contactid).getFormattedName(false, false) ;
                switchto = warningYesNoDialog(this, "Switch Contact", "Do you want to switch from " + fromname + " to " + toname + "?") ;
            }

            if (switchto) {
                populateDialog(contactid) ;

                if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
                    dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
                    ui->tabBar->setCurrentIndex((CALENDARTABPOS));
                }
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
    } else if (appt.isRepeating()) {
        // A repeat: which of its appointments to delete.  The entry is an occurrence, or the first appointment itself.
        QString hint = calendarlist.hintAt(idx) ;
        QDateTime occurrence = hint.contains('@') ? QDateTime::fromString(hint.section('@', 1), Qt::ISODate) : appt.getDate(Appointment::From) ;
        WarningChoice choice(this) ;
        choice.setText("Delete which appointments of this repeat?\n" + appt.asText()) ;
        choice.addChoice("Delete just this one", 'a', "The other appointments in the repeat are kept") ;
        choice.addChoice("Delete this one and all following", 'b', "The appointments before it are kept") ;
        choice.addChoice("Delete all in the repeat", 'c', "Every appointment in the repeat is deleted") ;
        choice.addChoice("Cancel", 'd', "Nothing is deleted") ;
        choice.setDefaultChoice(3) ;
        int chosen = choice.choose() ;
        int n = appt.indexOfOccurrence(occurrence) ;
        if (chosen==0 && n>=0) {
            appt.addException(occurrence) ;
        } else if (chosen==1 && n>0) {
            // The occurrences removed from here on are cancelled to the contact, as exceptions, which the messages notify.
            // Only the next ten are, the number the contact's list of upcoming appointments shows.
            QDateTime now = QDateTime::currentDateTimeUtc() ;
            int listed=0 ;
            for (int k=n; listed<10 && !appt.occurrenceEnded(k); k++) {
                QDateTime when = appt.occurrenceFrom(k) ;
                if (when>now) {
                    appt.addException(when) ;
                    listed++ ;
                }
            }
            appt.endBefore(n) ;
        } else if (chosen==2 || (chosen==1 && n==0)) {
            appt.setFlag(Appointment::Deleted, true) ;
        } else {
            return ;
        }
        calendar.save() ;
        LoadTabs() ;

    } else {
        // A single appointment is deleted
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
