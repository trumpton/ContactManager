#include "mainwindow.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <algorithm>

// Number of backup archives to keep, the newest ones
static const int backupsToKeep = 2 ;

// Makes the archive and waits for zip to finish, so the file is there when this returns.  zip's own helper starts zip and
// lets the process object be deleted at once, which stops zip before it has written anything.  True when the file exists.
static bool makeArchive(QString databasepath, QString filename)
{
    QStringList args ;
    args << "-r" << "-q" << filename << databasepath + "/contact" << databasepath + "/calendar" ;

    QProcess zipper ;
    zipper.start("zip", args) ;
    if (!zipper.waitForStarted()) return false ;
    zipper.waitForFinished(-1) ;
    return QFileInfo::exists(filename) ;
}

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

    // The backup folder may not exist yet (a new data folder): zip cannot write its archive into a folder that is not there
    QDir().mkpath(archivepath) ;

    QString filename = archivepath + QString::number(seq) + "-" + nowdate + "-backup.zip" ;
    if (!makeArchive(databasepath, filename)) {
        // The date is not recorded, so the next start tries again
        addLog("BACKUP FAILED: " + filename + " could not be written, so no backup is made. Check the backup folder is writable.") ;
        msg("Backup failed.") ;
        return ;
    }

    gConf->setBackupDate(nowdate) ;
    gConf->setBackupSequence(seq+1) ;

    removeOldBackups(archivepath) ;

    msg("Backup complete.") ;

}
