#ifndef TODO_H
#define TODO_H


#include <QString>

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

    bool save(QString path) ;
    bool load(QString path, QString idname) ;
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
