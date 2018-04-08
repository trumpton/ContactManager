
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
    ui->tabBar->setCurrentIndex(CALENDARTABPOS);
    ui->listCalendar->setFocus();
    ui->listCalendar->setCurrentIndex(calendarlist.getModel()->index(0)) ;

    QString thisid = calendar.getNearestAppointment().getField(Appointment::ID) ;
    int index = calendarlist.find(thisid) ;

    if (index>=0) {
        ui->listCalendar->setCurrentIndex(calendarlist.getModel()->index(index)) ;
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
        ui->tabBar->setCurrentIndex(TODOTABPOS);
        ui->editToDo->setFocus() ;
    }
}

