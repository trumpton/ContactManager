#include "search.h"
#include "ui_search.h"
#include "listviewstrings.h"
#include "../Lib/supportfunctions.h"

Search::Search(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Search)
{
    getSelectionResponse = "" ;
    ui->setupUi(this);
    clearResults() ;
    ui->listSearchResults->setModel(strings.getModel()) ;
}

void Search::clearResults()
{
    ui->listSearchResults->reset() ;
    strings.clearStrings() ;
    selectedindex = -1 ;
    entries = 0 ;
}

void Search::addString(QString resulttext, QString resulthint)
{
    strings.addString(resulttext, resulthint) ;
    dbg("Search::addString: listSearchResults->setFocus()") ;
    ui->listSearchResults->setFocus() ;
    entries++ ;
}

Search::~Search()
{
    delete ui;
}

QString Search::getSelection()
{
    int index = ui->listSearchResults->currentIndex().row() ;
    if (index>=0)
        getSelectionResponse = strings.hintAt(index) ;
    else
        getSelectionResponse = "" ;
    return getSelectionResponse ;
}

QString Search::getFirst()
{
    getSelectionResponse = "" ;
    if (entries>0) getSelectionResponse = strings.hintAt(0) ;
    return getSelectionResponse ;
}

void Search::on_listSearchResults_clicked(const QModelIndex &index)
{
    Q_UNUSED(index) ;
    // TODO: store index so that serc form can be queried for the ID
    //selectedindex = ui->listSearchResults->currentIndex().row() ;
}
