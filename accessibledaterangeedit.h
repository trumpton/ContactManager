#ifndef ACCESSIBLEDATERANGEEDIT_H
#define ACCESSIBLEDATERANGEEDIT_H

#include <QLineEdit>
#include <QDateTime>
#include <QKeyEvent>
#include <QFocusEvent>

class AccessibleDateRangeEdit : public QLineEdit
{

    Q_OBJECT

private:
    int pos ;
    int monthdelta ;
    QDateTime datetime ;
    int duration ;
    QString datetext, durationtext ;
    QString dateprefix, dateformat, format ;
    void keyPressEvent(QKeyEvent *event) ;
    void focusInEvent(QFocusEvent *event) ;
    void updateDateText() ;

public:
    explicit AccessibleDateRangeEdit(QWidget *parent = 0);
    void setDateTime(QDateTime newdatetime, int dur=-1) ;
    void setRange(QDateTime& from, QDateTime& to) ;
    int getDuration() ;
    QDateTime& getDateTime() ;

public: signals:
     void valueChanged(QDateTime datetime, int duration);

public slots:

};

#endif // ACCESSIBLEDATERANGEEDIT_H
