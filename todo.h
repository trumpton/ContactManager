#ifndef TODO_H
#define TODO_H


#include <QString>
#include "../Lib/encryption.h"

enum TodoOverviewType {
    todoAsText = 0,
    todoAsHTML = 1
};

class Todo
{

public:
    QString idName ;
    QString todoText ;
    bool dirty ;

private:
    QString getOverviewResult ;

public:

    Todo();
    ~Todo() ;

private:
    // Unused copy construct
    Todo(const Todo& other) ;

public:
    bool save(QString path, Encryption *enc) ;
    bool load(QString path, QString idname, Encryption *enc) ;
    bool createNew(QString idname) ;
    bool isdirty() ;
    bool isEmpty() ;

    Todo * getThis() ;

    int find(QString text, int startline=0) ;

    const QString& getText() ;
    bool setText(QString& newtext) ;

    const QString& getOverview(enum TodoOverviewType overviewtype)  ;

    Todo &operator=(const Todo &rhs);

};

#endif // TODO_H
