#include "mainwindow.h"
#include <QDir>
#include <algorithm>

// Number of backup archives to keep, the newest ones
static const int backupsToKeep = 2 ;

// Backups are named <sequence>-<yyyymmdd>-backup.zip, so the sequence number gives their order
static void removeOldBackups(QString archivepath)
{
    QDir dir(archivepath) ;

    // Only our own archives, so a file someone else put in the folder is never removed
    QStringList backups ;
    for (const QString &name : dir.entryList(QStringList() << "*-backup.zip", QDir::Files)) {
        bool isnumbered ;
        name.section('-', 0, 0).toInt(&isnumbered) ;
        if (isnumbered) backups << name ;
    }

    std::sort(backups.begin(), backups.end(), [](const QString &a, const QString &b) {
        return a.section('-', 0, 0).toInt() > b.section('-', 0, 0).toInt() ;
    }) ;

    for (int i=backupsToKeep; i<backups.size(); i++) {
        dir.remove(backups[i]) ;
    }
}

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

    removeOldBackups(archivepath) ;

    msg("Backup complete.") ;

}
