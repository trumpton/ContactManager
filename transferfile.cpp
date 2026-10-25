#include "transferfile.h"

#include <QFile>
#include <QXmlStreamReader>

bool TransferRecord::has(const QString &name) const
{
    for (const QPair<QString, QString> &field : fields) {
        if (field.first==name) return true ;
    }
    return false ;
}

QString TransferRecord::value(const QString &name) const
{
    for (const QPair<QString, QString> &field : fields) {
        if (field.first==name) return field.second ;
    }
    return QString() ;
}

// Characters that XML 1.0 can hold.  Other control characters are not allowed, even as references.
static bool xmlCanHold(QChar c)
{
    ushort u = c.unicode() ;
    return u==0x09 || u==0x0A || u==0x0D || (u>=0x20 && u<=0xFFFD) ;
}

QString xmlEscape(const QString &text)
{
    QString out ;
    out.reserve(text.size()) ;
    for (QChar c : text) {
        switch (c.unicode()) {
        case '&': out += "&amp;" ; break ;
        case '<': out += "&lt;" ; break ;
        case '>': out += "&gt;" ; break ;
        case '"': out += "&quot;" ; break ;
        case '\'': out += "&apos;" ; break ;
        case '\r': out += "&#13;" ; break ;
        default:
            out += xmlCanHold(c) ? c : QChar(0xFFFD) ;
        }
    }
    return out ;
}

QString csvEscape(const QString &text)
{
    bool spaceatend = !text.isEmpty() && (text.at(0).isSpace() || text.at(text.size()-1).isSpace()) ;
    if (!text.contains(',') && !text.contains('"') && !text.contains('\n') && !text.contains('\r') && !spaceatend) {
        return text ;
    }
    QString out = text ;
    out.replace("\"", "\"\"") ;
    return "\"" + out + "\"" ;
}

QList<QStringList> parseCsv(const QString &text)
{
    QList<QStringList> rows ;
    QStringList row ;
    QString cell ;
    bool inquotes = false ;

    for (int i=0; i<text.size(); i++) {
        QChar c = text.at(i) ;
        if (inquotes) {
            if (c=='"') {
                if (i+1<text.size() && text.at(i+1)=='"') { cell += '"' ; i++ ; }
                else inquotes = false ;
            } else {
                cell += c ;
            }
        } else if (c=='"') {
            inquotes = true ;
        } else if (c==',') {
            row << cell ;
            cell.clear() ;
        } else if (c=='\r' || c=='\n') {
            if (c=='\r' && i+1<text.size() && text.at(i+1)=='\n') i++ ;
            row << cell ;
            cell.clear() ;
            if (!(row.size()==1 && row.at(0).isEmpty())) rows << row ;
            row.clear() ;
        } else {
            cell += c ;
        }
    }

    // A last line with no line break at the end
    if (!cell.isEmpty() || !row.isEmpty()) {
        row << cell ;
        if (!(row.size()==1 && row.at(0).isEmpty())) rows << row ;
    }
    return rows ;
}

QString writeTransferFile(const QString &path, const QList<TransferRecord> &records, bool asXml)
{
    QString text ;

    if (asXml) {
        text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<contactmanager>\n" ;
        for (const TransferRecord &record : records) {
            text += "  <record type=\"" + xmlEscape(record.kind) + "\">\n" ;
            for (const QPair<QString, QString> &field : record.fields) {
                text += "    <field name=\"" + xmlEscape(field.first) + "\">" + xmlEscape(field.second) + "</field>\n" ;
            }
            text += "  </record>\n" ;
        }
        text += "</contactmanager>\n" ;
    } else {
        // The first column is the kind of record.  The other columns are the field names, in the order first met.
        QStringList columns = QStringList() << "record" ;
        for (const TransferRecord &record : records) {
            for (const QPair<QString, QString> &field : record.fields) {
                if (!columns.contains(field.first)) columns << field.first ;
            }
        }

        // A byte order mark, so that spreadsheets read the file as UTF-8
        text = QChar(0xFEFF) ;
        QStringList header ;
        for (const QString &column : columns) header << csvEscape(column) ;
        text += header.join(",") + "\r\n" ;

        for (const TransferRecord &record : records) {
            QStringList cells = QStringList() << csvEscape(record.kind) ;
            for (int n=1; n<columns.size(); n++) {
                cells << csvEscape(record.value(columns.at(n))) ;
            }
            text += cells.join(",") + "\r\n" ;
        }
    }

    QFile file(path) ;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return "The file could not be written: " + path ;
    }
    file.write(text.toUtf8()) ;
    file.close() ;
    if (file.error()!=QFile::NoError) {
        return "The file could not be written: " + path ;
    }
    return QString() ;
}

static QString readXml(const QString &text, QList<TransferRecord> &records)
{
    QXmlStreamReader xml(text) ;
    bool sawroot = false ;
    TransferRecord current ;

    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext() ;
        if (token==QXmlStreamReader::StartElement) {
            if (xml.name()==QLatin1String("contactmanager")) {
                sawroot = true ;
            } else if (xml.name()==QLatin1String("record")) {
                current = TransferRecord() ;
                current.kind = xml.attributes().value("type").toString() ;
            } else if (xml.name()==QLatin1String("field")) {
                QString name = xml.attributes().value("name").toString() ;
                QString value = xml.readElementText() ;
                current.add(name, value) ;
            }
        } else if (token==QXmlStreamReader::EndElement && xml.name()==QLatin1String("record")) {
            records << current ;
        }
    }

    if (xml.hasError()) {
        return QString("The XML file could not be read: %1 (line %2)").arg(xml.errorString()).arg(xml.lineNumber()) ;
    }
    if (!sawroot) {
        return "This is not a ContactManager export file (the root element is not contactmanager)." ;
    }
    return QString() ;
}

static QString readCsv(const QString &text, QList<TransferRecord> &records)
{
    QList<QStringList> rows = parseCsv(text) ;
    if (rows.isEmpty()) return "The file is empty." ;

    const QStringList &header = rows.at(0) ;
    if (header.isEmpty() || header.at(0)!=QLatin1String("record")) {
        return "This is not a ContactManager export file (the first column must be 'record')." ;
    }

    for (int r=1; r<rows.size(); r++) {
        const QStringList &row = rows.at(r) ;
        TransferRecord record ;
        record.kind = row.at(0) ;
        for (int n=1; n<row.size() && n<header.size(); n++) {
            record.add(header.at(n), row.at(n)) ;
        }
        records << record ;
    }
    return QString() ;
}

QString readTransferFile(const QString &path, QList<TransferRecord> &records)
{
    records.clear() ;

    QFile file(path) ;
    if (!file.open(QIODevice::ReadOnly)) {
        return "The file could not be opened: " + path ;
    }
    QString text = QString::fromUtf8(file.readAll()) ;
    file.close() ;

    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1) ;

    if (text.trimmed().startsWith('<')) return readXml(text, records) ;
    return readCsv(text, records) ;
}
