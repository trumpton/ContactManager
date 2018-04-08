
#include <QFile>
#include <QDateTime>
#include <QStringList>
#include <QTextStream>
#include "todo.h"


Todo::Todo()
{
    idName = "" ;
    dirty=false ;
    todoText = "" ;
    getOverviewResult="" ;
    createNew(idName) ;
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
    dirty=false ;
    todoText="" ;
    idName = idname ;
    return dirty ;
}

// TODO: Make this save a "safe save", i.e. save, check, rename
bool Todo::save(QString path)
{
    if (dirty && idName.compare("")!=0) {
        QFile file(path + idName + ".todolist");

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        QTextStream out(&file);
        out.setCodec("UTF-8") ;

        out << todoText ;
        file.close() ;
        dirty = false ;
    }
    return true ;
}

// TODO: Make this load a "safe load", i.e. check for failed save
bool Todo::load(QString path, QString idname)
{
    QString Line ;

    createNew(idname) ;
    QString filename = path + idName + ".todolist" ;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&file);
    in.setCodec("UTF-8") ;

    while (!in.atEnd()) {
       todoText = todoText + in.readLine() + "\n" ;
    }
    file.close() ;

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
    return *this ;
}

