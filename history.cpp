
#include <QFile>
#include <QDateTime>
#include <QStringList>
#include <QTextStream>
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
    QRegExp re(".*(" +text.toLower() +").*") ;
    QString overview = getOverview(historyAsText).toLower() ;
    if (re.exactMatch(overview)) return 1;
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
         out.setCodec("UTF-8") ;
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
    in.setCodec("UTF-8") ;

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
        QRegExp re("(\\d{1,2}[-\\s]+\\w{3,10}[-\\s]+\\d{2,4})[\\s-:]+(.*)$") ;
        for (int i=0; i<journallines.size() && (overviewtype==historyAsHTML ||
             (overviewtype==historyTopLinesAsHTML && i<5)); i++) {
            QString line = journallines.at(i) ;
            line = line.trimmed() ;
            if (!line.isEmpty()) {
                if (re.exactMatch(line)) {
                    getOverviewResult += "<b>" + re.cap(1) + ": </b><i>" + re.cap(2) + "</i><br/>\n" ;
                } else {
                    getOverviewResult += "&nbsp;&nbsp;" + line + "<br/>\n" ;
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
      Entry = Entry.replace(QRegExp("[\n\r \t]*\n[\n\r \t]*"), "; ") ;
      Entry = Entry.replace(QRegExp("[\\. ;]*$"), QString("")).trimmed() ;
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

