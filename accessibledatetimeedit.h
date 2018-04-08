#ifndef ACCESSIBLEDATETIMEEDIT_H
#define ACCESSIBLEDATETIMEEDIT_H

#include <QLineEdit>
#include <QDateTime>
#include <QKeyEvent>
#include <QFocusEvent>

class AccessibleDateTimeEdit : public QLineEdit
{
    Q_OBJECT

private:
    int pos ;
    int monthdelta ;
    bool partialok ;
    QDateTime datetime ;
    QString datetext ;
    QString dateprefix, dateformat ;
    void keyPressEvent(QKeyEvent *event) ;
    void focusInEvent(QFocusEvent *event) ;
    void updateDateText() ;

public:
    explicit AccessibleDateTimeEdit(QWidget *parent = 0);
    void setFormat(QString prefix, QString format, bool partial=false) ;
    void setDateTime(QDateTime &newdatetime) ;
    QDateTime& getDateTime() ;


signals:

public slots:

};

#endif // ACCESSIBLEDATETIMEEDIT_H
