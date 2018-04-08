#ifndef HISTORY_H
#define HISTORY_H


#include <QString>

enum HistoryOverviewType {
    historyAsText = 0,
    historyAsHTML = 1
};

class History
{

public:
    QString idName ;
    QString HistoryText ;
    bool dirty ;

private:
    QString getOverviewResult ;
    QString cachedPath ;

public:

    History();
    ~History() ;

    bool save(QString path = QString("") ) ;
    bool load(QString path, QString idname) ;
    bool createNew(QString idname) ;
    bool isdirty() ;
    bool isEmpty() ;

    History * getThis() ;

    int find(QString text, int startline=0) ;

    const QString& getHistory() ;
    bool addEntry(QString Entry) ;
    bool updateHistory(QString newhistory) ;

    QString getOverview(enum HistoryOverviewType overviewtype)  ;

    History &operator=(const History &rhs);

};

#endif // HISTORY_H
