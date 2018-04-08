
#include "mainwindow.h"

void MainWindow::LoadTodo()
{
    Contact& dr = db.getSelected() ;
    ui->editToDo->setPlainText("") ;
    if (dr.isNull() || dr.isEmpty()) return ;
    Todo& t = dr.getTodo() ;
    ui->editToDo->setPlainText(t.getText()) ;
}

void MainWindow::SaveTodo()
{
    Contact& dr = db.getSelected() ;
    if (dr.isNull() || dr.isEmpty()) return ;
    Todo& t = dr.getTodo() ;
    QString txt = ui->editToDo->toPlainText() ;
    t.setText(txt) ;
}
