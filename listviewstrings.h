#ifndef LISTVIEWSTRINGS_H
#define LISTVIEWSTRINGS_H

#include <QString>
#include <QStringList>
#include <QStringListModel>

class ListViewStrings
{

private:
    QStringList listtext ;
    QStringList listhint ;
    QStringListModel model ;

public:
    explicit ListViewStrings(QObject *parent = 0);
    ~ListViewStrings() ;

    QModelIndex findModelIndex(int idx) ;
    void addString(QString resulttext, QString resulthint) ;
    void clearStrings() ;
    int find(QString resulthint) ;
    int find(QString resulttext, int startindex) ;
    QString stringAt(int index) ;
    QString hintAt(int index) ;
    QStringListModel *getModel() ;

signals:

public slots:

};

#endif // LISTVIEWSTRINGS_H
