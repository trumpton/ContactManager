
#include <QRegularExpression>
#include "mainwindow.h"
#include "../Lib/alertsound.h"

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
// on_actionGotoToday_triggered
//
 void MainWindow::on_actionGotoToday_triggered()
 {
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


// The date typed for Goto Date, as day/month/year (25/01/2035, or 25/1/35).  Invalid if it is not a date.
static QDate parseGotoDate(QString text)
{
    QStringList parts = text.trimmed().split(QRegularExpression("[/.-]")) ;
    if (parts.size()!=3) return QDate() ;

    bool okday, okmonth, okyear ;
    int day = parts.at(0).toInt(&okday) ;
    int month = parts.at(1).toInt(&okmonth) ;
    int year = parts.at(2).toInt(&okyear) ;
    if (!okday || !okmonth || !okyear) return QDate() ;

    if (parts.at(2).length()<=2) year += 2000 ;
    return QDate(year, month, day) ;
}

//
// on_actionGotoDate_triggered
//
// Asks for a date, and shows its first entry in the calendar.  When the date has no entry, the entry is the first one on the
// nearest date before it, or the top of the list when there is no entry before it.
//
void MainWindow::on_actionGotoDate_triggered()
{
    bool ok = false ;
    QString today = QDate::currentDate().toString("dd/MM/yyyy") ;
    QString text = inputDialog(this, "Goto Date", "Date (day/month/year, for example 25/01/2035)", QLineEdit::Normal, today, &ok) ;
    if (!ok) return ;

    QDate wanted = parseGotoDate(text) ;
    if (!wanted.isValid()) {
        warningOkDialog(this, "Goto Date", "The date must be day/month/year, for example 25/01/2035.") ;
        return ;
    }

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

