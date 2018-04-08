
#include "mainwindow.h"
#include "help.h"
#include "../Lib/alertsound.h"

// HELP MENU


void MainWindow::on_actionAbout_triggered()
{
    class About *abt ;
    abt = new About(this) ;    
    play(Ok) ;
    abt->exec() ;
    delete abt ;
}


void MainWindow::on_actionManual_triggered()
{
    class Help *hlp ;
    hlp = new Help(this) ;
    play(Ok) ;
    hlp->exec() ;
    delete hlp ;
}


