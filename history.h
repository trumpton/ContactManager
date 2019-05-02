#ifndef HISTORY_H
#define HISTORY_H


#include <QString>
#include "../Lib/encryption.h"

enum HistoryOverviewType {
    historyAsText = 0,
    historyAsHTML = 1,
    historyTopLinesAsHTML
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
    Encryption *enc ;

private:
    bool createNew(QString idname) ;

public:

    History();
    ~History() ;
private:
    // Unused copy construct
    History(History& other) ;

public:
    bool save(QString path = QString("")) ;
    bool load(QString path, QString idname, Encryption *enc) ;
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
