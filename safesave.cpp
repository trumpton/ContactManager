#include <QFile>
#include <QFileInfo>
#include "safesave.h"
#include "../Lib/encryption.h"

// Puts the checked temporary file in place of path.  The old file is kept as a backup until the new
// one has been renamed into place, so a failed rename can be undone.
static bool replaceWithTemp(const QString &tempPath, const QString &path)
{
    QString backupPath = path + ".bak" ;
    bool hadOld = QFile::exists(path) ;

    QFile::remove(backupPath) ;
    if (hadOld && !QFile::rename(path, backupPath)) {
        QFile::remove(tempPath) ;
        return false ;
    }

    if (!QFile::rename(tempPath, path)) {
        if (hadOld) QFile::rename(backupPath, path) ;
        QFile::remove(tempPath) ;
        return false ;
    }

    if (hadOld) QFile::remove(backupPath) ;
    return true ;
}

bool safeSavePlain(const QString &path, const QByteArray &data)
{
    QString tempPath = path + ".tmp" ;

    QFile file(tempPath) ;
    if (!file.open(QIODevice::WriteOnly)) return false ;
    bool written = (file.write(data) == data.size()) ;
    file.close() ;

    // Check: the whole of the data is on disk before the good file is replaced
    if (!written || QFileInfo(tempPath).size() != data.size()) {
        QFile::remove(tempPath) ;
        return false ;
    }

    return replaceWithTemp(tempPath, path) ;
}

bool safeSaveEncrypted(Encryption *enc, const QString &path, const QByteArray &data)
{
    QString tempPath = path + ".tmp" ;

    if (!enc->save(tempPath, data)) {
        QFile::remove(tempPath) ;
        return false ;
    }

    // Check: the file is there, and starts with the encrypted-file header Encryption::save writes
    QFile file(tempPath) ;
    if (!file.open(QIODevice::ReadOnly)) return false ;
    QByteArray header = file.read(16) ;
    file.close() ;
    if (!header.startsWith("&!ENCRY-") || QFileInfo(tempPath).size() <= header.size()) {
        QFile::remove(tempPath) ;
        return false ;
    }

    return replaceWithTemp(tempPath, path) ;
}
