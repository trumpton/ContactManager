
#include "mainwindow.h"
#include "../Lib/alertsound.h"

void MainWindow::LoadEditHistory()
{
    Contact& dr = db.getSelected() ;
    ui->editHistory->setPlainText("") ;
    if (dr.isNull() || dr.isEmpty()) return ;
    History& h = dr.getHistory() ;
    ui->editHistory->setPlainText(h.getHistory()) ;
}

void MainWindow::SaveEditHistory()
{
    Contact& dr = db.getSelected() ;
    if (dr.isNull() || dr.isEmpty()) return ;
    History& h = dr.getHistory() ;
    h.updateHistory(ui->editHistory->toPlainText()) ;
}

// Append to history, updating ui if necessary
void MainWindow::appendHistory(Contact& contact, QString msg)
{
    if (contact.isNull() || contact.isEmpty()) return ;
    if (contact.getField(Contact::ID).compare(db.getSelected().getField(Contact::ID))==0) {
        SaveEditHistory() ;
        contact.getHistory().addEntry(msg)  ;
        LoadEditHistory() ;
        LoadOverviewTab() ;
    } else {
        contact.getHistory().addEntry(msg)  ;
    }
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
        ui->editHistory->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::TextEditable) ;

        if (ui->tabBar->currentIndex()!=HISTORYTABPOS) {
            dbg("tabBar->setCurrentIndex(HISTORYTABPOS)") ;
            ui->tabBar->setCurrentIndex(HISTORYTABPOS);
        }

        dbg("editHistory->setFocus()") ;
        ui->editHistory->setFocus() ;
    }
}
