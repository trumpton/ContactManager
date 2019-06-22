#include "search.h"
#include "ui_search.h"
#include "../Lib/supportfunctions.h"

Search::Search(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Search)
{
    ui->setupUi(this);
    ui->listSearchResults->setModel(&strings) ;
    clearResults() ;
}

void Search::clearResults()
{
    ui->listSearchResults->reset() ;
    strings.clear() ;
}

void Search::addString(QString resulttext, QString resulthint)
{
    strings.appendData(resulttext, resulttext, resulthint) ;
    dbg("Search::addString: listSearchResults->setFocus()") ;
    ui->listSearchResults->setFocus() ;
}

Search::~Search()
{
    delete ui;
}

QString Search::getSelection()
{
    int index = ui->listSearchResults->currentIndex().row() ;
    if (index>=0) return strings.hintAt(index) ;
    else return QString("") ;
}

QString Search::getFirst()
{
    if (strings.rowCount()>0)  return strings.hintAt(0) ;
    else return QString("") ;
}

