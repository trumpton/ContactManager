
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

     Appointment nearest = calendar.getNearestAppointment() ;
     QString thisid = nearest.getField(Appointment::ID) ;
     QModelIndex index = calendarlist.find(thisid) ;

     if (!index.isValid()) {
         // No nearest entry found, goto end of list
         int rows = calendarlist.rowCount() ;
         index = calendarlist.index(rows-1,0) ;
     }

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

