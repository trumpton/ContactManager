#include "mainwindow.h"

void MainWindow::doBackup()
{
    QString databasepath = gConf->getDatabasePath() ;
    QString archivepath = gConf->getBackupPath() ;

    if (databasepath.isEmpty()) return ;
    if (archivepath.isEmpty()) return ;

    QDateTime t = QDateTime::currentDateTime() ;
    QString nowdate = t.toString("yyyyMMdd") ;

    int seq = gConf->getBackupSequence();
    QString lastdate = gConf->getBackupDate() ;

    if (nowdate.compare(lastdate)==0) return ;

    msg("Backing up ....") ;

    QString filename = archivepath + QString::number(seq) + "-" + nowdate + "-backup.zip" ;
    zip(databasepath, filename) ;

    gConf->setBackupDate(nowdate) ;
    gConf->setBackupSequence(seq+1) ;

    msg("Backup complete.") ;

}
