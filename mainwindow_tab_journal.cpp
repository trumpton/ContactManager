
#include "mainwindow.h"
#include "../Lib/alertsound.h"

void MainWindow::LoadEditHistory()
{
    Contact& dr = db.getSelected() ;
    ui->editNotes->setPlainText("") ;
    if (dr.isNull() || dr.isEmpty()) return ;
    History& h = dr.getHistory() ;
    ui->editNotes->setPlainText(h.getHistory()) ;
}

void MainWindow::SaveEditHistory()
{
    Contact& dr = db.getSelected() ;
    if (dr.isNull() || dr.isEmpty()) return ;
    History& h = dr.getHistory() ;
    h.updateHistory(ui->editNotes->toPlainText()) ;
}

//
// on_actionEnableJournalHistoryEdit_triggered
//
//
void MainWindow::on_actionEnableEditHistoryEdit_triggered()
{
    if (db.getSelected().isNull()) {
        play(Disabled) ;
    } else {
        ui->editNotes->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::TextEditable) ;

        if (ui->tabBar->currentIndex()!=HISTORYTABPOS)
            ui->tabBar->setCurrentIndex(HISTORYTABPOS);

        ui->editNotes->setFocus() ;
    }
}
