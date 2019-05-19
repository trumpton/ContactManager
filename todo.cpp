
#include <QFile>
#include <QDateTime>
#include <QStringList>
#include <QTextStream>
#include "todo.h"
#include "configuration.h"
#include "../../Lib/supportfunctions.h"


Todo::Todo()
{
    createNew("") ;
}

Todo::~Todo()
{
}

bool Todo::isdirty()
{
    return dirty ;
}

bool Todo::isEmpty()
{
    return todoText.isEmpty() ;
}


// Search for string, and return -1 on failure
int Todo::find(QString text, int startline)
{
    Q_UNUSED(startline) ;
    QRegExp re(".*(" +text.toLower() +").*") ;
    QString overview = getText().toLower() ;
    if (re.exactMatch(overview)) return 1;
    else  return -1 ;
}

bool Todo::createNew(QString idname)
{
    cachedPath = "" ;
    dirty=false ;
    todoText="" ;
    idName = idname ;
    getOverviewResult="" ;
    return true ;
}

// TODO: Make this save a "safe save", i.e. save, check, rename
bool Todo::save(QString path)
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
        out << todoText ;
        out.flush();

        if (gConf->encryptedEnabled()) {
            // Save Encrypted
            if (!enc->save(path + idName + ".ztodolist", data)) {
                return false ;
            }
        } else {
            // Save Plaintext
            QFile file(path + idName + ".todolist");
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                return false;
            file.write(data) ;
            file.close() ;
        }

        dirty = false ;
    }
    return true ;
}

// TODO: Make this load a "safe load", i.e. check for failed save
bool Todo::load(QString path, QString idname)
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
        if (!enc->load(path + idname + ".ztodolist", data)) {
            dbg(QString("Loading %1.ztodolist FAILED").arg(idname)) ;
            return false ;
        }

    } else {

        // Load Plaintext File
        QFile file(path + idname + ".todolist");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        data = file.readAll() ;
        file.close() ;

    }

    QTextStream in(&data);
    in.setCodec("UTF-8") ;

    while (!in.atEnd()) {
       todoText = todoText + in.readLine() + "\n" ;
    }

    dirty = false ;
    return true ;
}

const QString& Todo::getOverview(enum TodoOverviewType overviewtype)
{
    getOverviewResult = "" ;
    if (overviewtype==todoAsHTML) {
        QStringList lines = getText().split("\n") ;
        Q_FOREACH(QString line, lines) {
            line = line.trimmed() ;
            if (!line.isEmpty()) {
                getOverviewResult += line + "<br/>\n" ;
            }
         }
    } else {
        getOverviewResult = getText() ;
    }
    return getOverviewResult ;

}


const QString& Todo::getText()
{
    return todoText ;
}


bool Todo::setText(QString &newtext)
{
    dirty = (todoText.compare(newtext)!=0) ;
    if (dirty) { todoText = newtext ; }
    return dirty ;
}


Todo* Todo::getThis()
{
    return this ;
}


Todo& Todo::operator=(const Todo &rhs)
{
    if (this == &rhs) return *this;

    this->idName = rhs.idName ;
    this->todoText = rhs.todoText ;
    this->dirty = rhs.dirty ;
    this->getOverviewResult = rhs.getOverviewResult ;
    this->cachedPath = rhs.cachedPath ;
    return *this ;
}

