
#include "mainwindow.h"
#include "../Lib/alertsound.h"
#include "../Lib/dateinputdialog.h"

//
// on_actionGotoMe_triggered
//
//
void MainWindow::on_actionGotoMe_triggered()
{
    QString me = gConf->getMe() ;
    if (me.isEmpty()) {
        errorOkDialog(this, "Error, Not Configured", "Me! not configured, so I can't find my list") ;
    } else {
        populateDialog(me) ;
    }
}

//
// calendarFilterOff
//
// Moving the date turns off the contact filter (issue #92), so the list shows all entries again.  The switch sound is played
// when the filter was on.
//
void MainWindow::calendarFilterOff()
{
    if (!calendarcontactonly) return ;
    calendarcontactonly = false ;
    play(SwitchOff) ;
    LoadCalendarTab() ;
    msg("Showing all appointments") ;
}

//
// on_actionGotoToday_triggered
//
void MainWindow::on_actionGotoToday_triggered()
{
    calendarFilterOff() ;

    if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
        dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
        ui->tabBar->setCurrentIndex(CALENDARTABPOS);
    }

    dbg("listCalendar->setFocus()") ;
    ui->listCalendar->setFocus();

    dbg("listCalendar->setCurrentIndex()") ;
    ui->listCalendar->setCurrentIndex(calendarlist.index(0,0)) ;

    // The nearest entry is the first one in the list that has not ended, repeats included.  If there is none, the end.
    QDateTime now = QDateTime::currentDateTimeUtc() ;
    int nearestrow = -1 ;
    for (int r=0; r<calendarrowends.size() && nearestrow<0; r++) {
        if (calendarrowends[r] > now) nearestrow = r ;
    }
    QModelIndex index = calendarlist.index(nearestrow>=0 ? nearestrow : calendarlist.rowCount()-1, 0) ;

    if (index.isValid()) {
        dbg("listCalendar->setCurrentIndex()") ;
        ui->listCalendar->setCurrentIndex(index) ;
    }
}

//
// gotoCalendarDate
//
// Shows the first entry on a date in the calendar.  When the date has no entry, the entry is the first one on the nearest
// date before it, or the top of the list when there is no entry before it.
//
void MainWindow::gotoCalendarDate(QDate wanted)
{
    if (ui->tabBar->currentIndex()!=CALENDARTABPOS) {
        dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
        ui->tabBar->setCurrentIndex(CALENDARTABPOS);
    }

    dbg("listCalendar->setFocus()") ;
    ui->listCalendar->setFocus();

    // The rows are in start order, so the first row on the date is found first.  Otherwise the row is the first on the
    // latest date before it.  Otherwise, row 0 (the top of the list).
    int row = -1 ;
    QDate before ;
    for (int r=0; r<calendarrowstarts.size(); r++) {
        QDate day = calendarrowstarts[r].toLocalTime().date() ;
        if (day==wanted) {
            row = r ;
            break ;
        }
        if (day<wanted && (!before.isValid() || day>before)) {
            before = day ;
            row = r ;
        }
    }
    if (row<0) row = 0 ;

    QModelIndex index = calendarlist.index(row,0) ;
    if (index.isValid()) {
        dbg("listCalendar->setCurrentIndex()") ;
        ui->listCalendar->setCurrentIndex(index) ;
    }
}

//
// on_actionGotoDate_triggered
//
// Asks for a date with the date dialog (Control-D, or D in the calendar list), and goes to it.
//
void MainWindow::on_actionGotoDate_triggered()
{
    bool ok = false ;
    QDate wanted = dateInputDialog(this, "Goto Date", "Go to date", QDate::currentDate(), &ok) ;
    if (!ok) return ;

    calendarFilterOff() ;
    gotoCalendarDate(wanted) ;
}

//
// gotoTypedCalendarDate
//
// A digit (0 to 3) typed in the calendar list opens a text entry with the digit in it, and the rest of the date is typed after
// it (issue #92).  Enter goes to the date.  An entry that does not look like a date closes the entry and beeps, and nothing
// moves.  An empty entry, or Escape, closes it with no beep.
//
void MainWindow::gotoTypedCalendarDate(QString typed)
{
    QString prompt = "Type a date, such as 10 oct 2026, 1 nov 2027, or 2 march for the next one.  Press Enter to go to it." ;

    // The entry is shown with its cursor at the end and no text selected, so the next key adds to the digits typed
    QInputDialog dialog(this) ;
    dialog.setWindowTitle("Go to date") ;
    dialog.setLabelText(prompt) ;
    dialog.setTextValue(typed) ;
    QLineEdit *edit = dialog.findChild<QLineEdit*>() ;
    if (edit) {
        QObject::connect(edit, &QLineEdit::selectionChanged, edit, [edit]() {
            if (edit->hasSelectedText()) QTimer::singleShot(0, edit, [edit]() { edit->deselect() ; edit->end(false) ; }) ;
        }) ;
        edit->end(false) ;
    }

    play(Query) ;
    if (dialog.exec()!=QDialog::Accepted) return ;
    QString text = dialog.textValue() ;

    // An empty entry, pressed Enter on, is the same as Escape
    if (text.trimmed().isEmpty()) return ;

    QDate wanted ;
    if (!parseCalendarDate(text, QDate::currentDate(), &wanted)) {
        play(Error) ;
        return ;
    }

    calendarFilterOff() ;
    gotoCalendarDate(wanted) ;
}

//
// gotoContactAppointment
//
// The next (step 1) or previous (step -1) entry in the calendar list for the open contact, from the entry on screen.  With no
// contact open, or no such entry, the list stays where it is, and a beep is played.
//
void MainWindow::gotoContactAppointment(int step)
{
    Contact &open = db.getSelected() ;
    QString openid = open.getField(Contact::ID) ;
    bool contactopen = !open.isNull() && !openid.isEmpty() && openid.compare(SELECT_OVERVIEW)!=0 ;
    if (!contactopen) {
        play(Error) ;
        msg("Open a contact first") ;
        return ;
    }

    int row = ui->listCalendar->currentIndex().row() ;
    for (int r=row+step; r>=0 && r<calendarrowcontacts.size(); r+=step) {
        if (calendarrowcontacts.at(r)==openid) {
            ui->listCalendar->setCurrentIndex(calendarlist.index(r,0)) ;
            return ;
        }
    }
    play(Error) ;
    msg(step>0 ? "No later appointment for this contact" : "No earlier appointment for this contact") ;
}

// Jump to TODO List
void MainWindow::on_actionGotoMyToDoList_triggered()
{
    on_actionGotoMe_triggered() ;
    if (!gConf->getMe().isEmpty()) on_actionGotoToDoList_triggered() ;
}

void MainWindow::on_actionGotoToDoList_triggered()
{
    Contact &c = db.getSelected() ;
    if (c.isNull()) {
        // Safety check
        errorOkDialog(this, "Debug", "on_actionGotoToDoList_triggered") ;
        return ;
    } else {
        if (ui->tabBar->currentIndex()!=TODOTABPOS) {
            dbg("tabBar->setCurrentIndex(TODOTABPOS)") ;
            ui->tabBar->setCurrentIndex(TODOTABPOS);
        }
        dbg("editToDo->setFocus()") ;
        ui->editToDo->setFocus() ;
    }
}

