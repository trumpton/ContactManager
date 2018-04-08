#include "listviewstrings.h"



ListViewStrings::~ListViewStrings()
{
}

QStringListModel *ListViewStrings::getModel()
{
   return &model ;
}


ListViewStrings::ListViewStrings(QObject *parent)
{
    Q_UNUSED(parent) ;
    listtext.clear() ;
    listhint.clear() ;
    model.setStringList(listtext) ;
}

QModelIndex ListViewStrings::findModelIndex(int idx)
{
    return model.index(idx) ;
}

void ListViewStrings::addString(QString resulttext, QString resulthint)
{
  listtext.append(resulttext) ;
  listhint.append(resulthint) ;
  model.setStringList(listtext) ;  // Probably not needed
}

void ListViewStrings::clearStrings()
{
  listtext.clear() ;
  listhint.clear() ;
  model.setStringList(listtext) ;  // Probably not needed
}

int ListViewStrings::find(QString resulthint)
{
  int i ;
  for (i=0; i<listhint.size(); i++)
      if (listhint.at(i).compare(resulthint)==0) return i ;
  return -1 ;
}

// TODO: make this a best match
int ListViewStrings::find(QString resulttext, int startindex)
{
    QRegExp rx ;
    int i=startindex+1 ;
    rx.setPattern(".*" + resulttext.toLower() + ".*") ;
    while (i<listtext.size()) {
        if (rx.indexIn(listtext.at(i).toLower())>=0) return i ;
        i++ ;
    }
    return -1 ;
}


QString ListViewStrings::stringAt(int index)
{
   if (index<0 || index>=listtext.size()) return "" ;
   else return listtext.at(index) ;
}

QString ListViewStrings::hintAt(int index)
{
    if (index<0 || index>=listhint.size()) return "" ;
    else return listhint.at(index) ;
}

