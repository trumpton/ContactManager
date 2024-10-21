
#include <QFile>
#include <QDateTime>
#include <QStringList>
#include <QTextStream>
#include <QRegularExpression>
#include "history.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"


History::History()
{
    createNew("") ;
}

History::~History()
{
}

bool History::isdirty()
{
    return dirty ;
}

bool History::isEmpty()
{
    return HistoryText.isEmpty() ;
}

// Search for string, and return -1 on failure
int History::find(QString text, int startline)
{
    Q_UNUSED(startline) ;
    QRegularExpression re(".*?(" +text.toLower() +").*?") ;
    QRegularExpressionMatch rem ;
    QString overview = getOverview(historyAsText).toLower() ;
    rem = re.match(overview) ;
    if (rem.hasMatch()) return 1;
    else  return -1 ;
}

bool History::createNew(QString idname)
{
    dirty=false ;
    HistoryText="" ;
    idName = idname ;
    cachedPath = "" ;
    getOverviewResult="" ;
    return true ;
}

// TODO: Make this save a "safe save", i.e. save, check, rename
bool History::save(QString path)
{
    Encryption *enc = gConf->encryption() ;
    if (!enc) {
        dbg("ERROR - call without enc") ;
        return false ;
    }

    if (dirty && idName.compare("")!=0) {

        if (path.isEmpty()) {
            path = cachedPath ;
        }

        if (path.isEmpty()) {
            dbg("ERROR - save with empty path called") ;
            return false ;
        }

        QByteArray data ;
         QTextStream out(&data, QIODevice::WriteOnly);
         out.setEncoding(QStringConverter::Utf8) ;
         out << HistoryText ;
         out.flush();

         if (gConf->encryptedEnabled()) {
             // Save encrypted
             dirty = !enc->save(path + idName + ".zhistory", data) ;
         } else {
             // Save Plaintext
             QFile file(path + idName + ".history");
             if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                 return false;
             file.write(data) ;
             file.close() ;
         }

    }
    return !dirty ;
}

// TODO: Make this load a "safe load", i.e. check for failed save
bool History::load(QString path, QString idname)
{
    Encryption *enc = gConf->encryption() ;
    if (!enc) {
        dbg("ERROR - call without enc") ;
        return false ;
    }

    createNew(idname) ;
    cachedPath = path ;

    QByteArray data ;

    if (gConf->encryptedEnabled()) {

        // Load Encrypted File
        if (!enc->load(path + idname + ".zhistory", data)) {
            dbg(QString("Loading %1.zhistory FAILED").arg(idname)) ;
            return false ;
        }

    } else {

        // Load Plaintext File
        QFile file(path + idname + ".history");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        data = file.readAll() ;
        file.close() ;

    }


    QTextStream in(&data);
    in.setEncoding(QStringConverter::Utf8) ;

    while (!in.atEnd()) {
       HistoryText = HistoryText + in.readLine() + "\n" ;
    }

    dirty = false ;
    return true ;
}

QString History::getOverview(enum HistoryOverviewType overviewtype)
{
    getOverviewResult = "" ;

    if (overviewtype==historyAsHTML || overviewtype==historyTopLinesAsHTML) {
        QString hist = getHistory() ;
        QStringList journallines = hist.replace("[\n]+", "\n").split("\n") ;

        // TODO: check this match string for move from 5.15 to 6.24
        QRegularExpression re("(\\d{1,2}[-\\s]+\\w{3,10}[-\\s]+\\d{2,4})[\\s-:]+(.*)$") ;
        QRegularExpressionMatch rem ;

        for (int i=0; i<journallines.size() && (overviewtype==historyAsHTML ||
             (overviewtype==historyTopLinesAsHTML && i<5)); i++) {
            QString line = journallines.at(i) ;
            line = line.trimmed() ;
            if (!line.isEmpty()) {

                rem = re.match(line) ;
                if (rem.hasMatch()) {
                    getOverviewResult += "<p><b>" + rem.captured(1) + ": </b><i>" + rem.captured(2) + "</i></p>\n" ;
                } else {
                    getOverviewResult += "<p>&nbsp;&nbsp;" + line + "</p>\n" ;
                }
            }
         }
    } else {
        getOverviewResult = getHistory() ;
    }
    return getOverviewResult ;

}


const QString& History::getHistory()
{
    return HistoryText ;
}


bool History::addEntry(QString Entry)
{
    QString NewEntry ;
    QDateTime today ;

   if (Entry.compare("")!=0) {
      Entry = Entry.replace(QRegularExpression("[\n\r \t]*\n[\n\r \t]*"), "; ") ;
      Entry = Entry.replace(QRegularExpression("[\\. ;]*$"), QString("")).trimmed() ;
      NewEntry +=  today.currentDateTime().toString("dd-MMM-yyyy: ") + Entry + ".\n";
      HistoryText = NewEntry + HistoryText ;
      dirty = true ;
      save() ;
   }
    return dirty ;
}


bool History::updateHistory(QString NewHistory)
{
    dirty = (HistoryText.compare(NewHistory)!=0) ;
    HistoryText = NewHistory ;
    return dirty ;
}



History* History::getThis()
{
    return this ;
}


History& History::operator=(const History &rhs)
{
    if (this == &rhs) return *this;

    this->idName = rhs.idName ;
    this->HistoryText = rhs.HistoryText ;
    this->dirty = rhs.dirty ;
    this->cachedPath = rhs.cachedPath ;
    this->getOverviewResult = rhs.getOverviewResult ;
    return *this ;
}

