#ifndef TRANSFERFILE_H
#define TRANSFERFILE_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

// One record in an export file: its kind ("contact" or "appointment"), and its fields as name / value
// pairs in the order they are written.
struct TransferRecord {
    QString kind ;
    QList<QPair<QString, QString> > fields ;

    void add(const QString &name, const QString &value) { fields << QPair<QString, QString>(name, value) ; }
    bool has(const QString &name) const ;
    QString value(const QString &name) const ;
} ;

// Escapes text for use in an XML element or attribute.  The five markup characters become entities, and
// a carriage return is written as a character reference so that it is not lost.  Characters that XML
// cannot hold at all are replaced with U+FFFD.
QString xmlEscape(const QString &text) ;

// Quotes a CSV field when it holds a comma, a quote, a line break, or a space at either end.  Quotes inside
// the field are doubled.
QString csvEscape(const QString &text) ;

// Splits CSV text into rows of fields.  Handles quoted fields with commas, quotes and line breaks.  Blank
// lines are skipped.
QList<QStringList> parseCsv(const QString &text) ;

// Writes the records to path, as XML or as CSV (UTF-8, with a byte order mark).  Returns an error message,
// or an empty string if the file was written.
QString writeTransferFile(const QString &path, const QList<TransferRecord> &records, bool asXml) ;

// Reads an XML or CSV export file, detected from its contents.  Returns an error message, or an empty
// string if the file was read.
QString readTransferFile(const QString &path, QList<TransferRecord> &records) ;

#endif // TRANSFERFILE_H
