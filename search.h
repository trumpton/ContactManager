#ifndef SEARCH_H
#define SEARCH_H

#include <QDialog>
#include <QStringList>
#include "listviewstrings.h"

namespace Ui {
class Search;
}

class Search : public QDialog
{
    Q_OBJECT
    
private:
    QString getSelectionResponse ;

public:
    explicit Search(QWidget *parent = 0);
    void clearResults() ;
    void addString(QString resulttext, QString resulthint) ;
    ~Search();
    QString getSelection() ;
    QString getFirst() ;


private slots:
    void on_listSearchResults_clicked(const QModelIndex &index);

private:
    Ui::Search *ui;
    ListViewStrings strings ;
    int selectedindex ;
    int entries ;

};

#endif // SEARCH_H
