#ifndef SAFESAVE_H
#define SAFESAVE_H

#include <QByteArray>
#include <QString>

class Encryption ;

// Safe saves: the data is written to a temporary file beside the target, checked, and only then
// swapped into place, so a failed save never leaves a partly written file where the good one was.
bool safeSavePlain(const QString &path, const QByteArray &data) ;
bool safeSaveEncrypted(Encryption *enc, const QString &path, const QByteArray &data) ;

#endif // SAFESAVE_H
