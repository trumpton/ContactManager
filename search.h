#ifndef SEARCH_H
#define SEARCH_H

#include <QDialog>
#include <QStringList>
#include "accessiblestringlistmodel.h"

namespace Ui {
class Search;
}

class Search : public QDialog
{
    Q_OBJECT
    
public:
    explicit Search(QWidget *parent = 0);
    ~Search();
    void clearResults() ;
    void addString(QString resulttext, QString resulthint) ;
    QString getSelection() ;
    QString getFirst() ;


private:
    Ui::Search *ui;
    AccessibleStringListModel strings ;

};

#endif // SEARCH_H
