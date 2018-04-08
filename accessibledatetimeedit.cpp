#include "accessibledatetimeedit.h"
#include <QApplication>
#include "../Lib/alertsound.h"

// Displays a date and time in the following format:
//
// DDD  DD / MMM / YYYY  HH : MM
//

//
// Intercepts arrow keys, and allows up/down changing of each field.
// If the field is numeric, allows direct entry.
//
// Checks values at each step, and updates all fields dynamically
//
// Intercepts tab, and directs to the next or previous sub-field
//

AccessibleDateTimeEdit::AccessibleDateTimeEdit(QWidget *parent) :
    QLineEdit(parent)
{
    partialok=false ;
    pos=0 ;
    monthdelta=-1 ;
    dateprefix = "ddd " ;
    dateformat =  dateprefix + "dd/MMM/yyyy hh:mm" ;
    QDateTime now = QDateTime::currentDateTime() ;
    setDateTime(now) ;
    updateDateText() ;
}

void AccessibleDateTimeEdit::setFormat(QString prefix, QString format, bool partial)
{
    dateprefix = prefix ;
    dateformat = dateprefix + format ;
    partialok = partial ;
    updateDateText() ;
}

//
// set and get the date/time
//
void AccessibleDateTimeEdit::setDateTime(QDateTime& newdatetime)
{
    datetime = newdatetime.toLocalTime() ;
    if (partialok && datetime.date().year()<10 && datetime.date().year()!=1 && datetime.date().year()!=4) {
        datetime = datetime.addYears(4-datetime.date().year()) ;
    }

    updateDateText() ;
}

QDateTime& AccessibleDateTimeEdit::getDateTime()
{
    return datetime ;
}

// Private
void AccessibleDateTimeEdit::updateDateText()
{

    datetext = datetime.toLocalTime().toString(dateformat) ;

    if (partialok) {
        bool isempty = (datetime.date().year()==1 && datetime.date().month()==1 && datetime.date().day()==1) ;
        bool noyear = datetime.date().year()==4 ;
        for (int i=0; i<dateformat.length(); i++) {
            QChar ch = dateformat.at(i) ;
            if (ch.isLetter() && isempty) datetext.replace(i, 1, '-') ;
            if (ch=='y' && noyear) datetext.replace(i, 1, '-') ;
        }
    }

    this->setText(datetext) ;
    this->setCursorPosition(pos) ;
}

void AccessibleDateTimeEdit::focusInEvent(QFocusEvent * event)
{
    updateDateText() ;
    QLineEdit::focusInEvent(event) ;
    this->deselect() ;
    pos=dateprefix.size() ;
    this->setCursorPosition(pos) ;
}

//
// Only allow keypresses at the correct position
//
void AccessibleDateTimeEdit::keyPressEvent(QKeyEvent *event)
{
    int invalid=false ;

    Qt::KeyboardModifiers keyMod = QApplication::keyboardModifiers();
    int key = event->key() ;
    bool control = keyMod.testFlag(Qt::ControlModifier) ;
    bool alt = keyMod.testFlag(Qt::AltModifier) ;
    bool shift = keyMod.testFlag(Qt::ShiftModifier) ;

    pos = this->cursorPosition() ;
    if (pos<0 || pos>=dateformat.size()) pos=0 ;

    QDateTime newtime = datetime ;
    int newpos = pos ;

    if (    shift || control || alt ||
            key == Qt::Key_Tab ||
            key == Qt::Key_Control ||
            key == Qt::Key_Alt ||
            key == Qt::Key_Shift ||
            key == Qt::Key_Enter ||
            key == Qt::Key_Escape ||
            key == Qt::Key_Return) {

        // default handler for control signals
        QLineEdit::keyPressEvent(event);
        return ;


    } else if(event->key() == Qt::Key_Minus && partialok) {

        // Remove date, or Set year
        QChar ch = dateformat.at(pos) ;
        if (!ch.isNull()) {
            if (!ch.isLetter()) ch=dateformat.at(pos-1) ;
            if (ch == 'd' || ch=='M' || ch=='h' || ch=='m') {
                newtime = datetime.fromString("010100010000", "ddMMyyyyhhmm") ;
            }
            if (ch=='y') newtime = newtime.addYears(4 - datetime.date().year()) ;
        }
        monthdelta=-1 ;

    } else if(event->key() == Qt::Key_Up) {

        // next value for current field
        QChar ch = dateformat.at(pos) ;
        if (!ch.isNull()) {
            if (!ch.isLetter()) ch=dateformat.at(pos-1) ;
            if (ch == 'd') newtime = newtime.addDays(1) ;
            else if (ch == 'M') newtime = newtime.addMonths(1) ;
            else if (ch == 'y') newtime = newtime.addYears(1) ;
            else if (ch == 'h') newtime = newtime.addSecs(3600) ;
            else if (ch == 'm') newtime = newtime.addSecs(60) ;
        }
        monthdelta=-1 ;

    }
    else if(event->key() == Qt::Key_Down) {

        // previous value for current field
        QChar ch = dateformat.at(pos) ;
        if (!ch.isNull()) {
            if (!ch.isLetter()) ch=dateformat.at(pos-1) ;
            if (ch == 'd') newtime = newtime.addDays(-1) ;
            else if (ch == 'M') newtime = newtime.addMonths(-1) ;
            else if (ch == 'y') newtime = newtime.addYears(-1) ;
            else if (ch == 'h') newtime = newtime.addSecs(-3600) ;
            else if (ch == 'm') newtime = newtime.addSecs(-60) ;
        }
        monthdelta=-1 ;

    }
    else if(event->key() == Qt::Key_Left) {

        // move left
        QChar ch = dateformat.at(pos) ;
        while (pos>0 && dateformat.at(pos)==ch) pos-- ;
        while (pos>0 && !dateformat.at(pos).isLetter()) pos-- ;
        if (pos<dateprefix.size()) {
            pos=dateformat.size()-1 ;
        }
        while (pos>0 && dateformat.at(pos).isLetter()) pos-- ;
        if (!dateformat.at(pos).isLetter()) pos++ ;
        newpos=pos ;
        monthdelta=-1 ;

    }
    else if(event->key() == Qt::Key_Right) {

        // move right
        QChar ch = dateformat.at(pos) ;
        while (pos<datetext.size() && dateformat.at(pos)==ch) pos++ ;
        while (pos<datetext.size() && !dateformat.at(pos).isLetter()) pos++ ;
        if (pos>=datetext.size()) { pos=dateprefix.size() ; }
        newpos=pos ;
        monthdelta=-1 ;

    }
    else if (key>=Qt::Key_0 && key<=Qt::Key_9 && pos<(dateformat.size())) {

        // handle a numeric input to change the month
        if (dateformat.at(pos)=='M') {
            if (monthdelta==-1) {
                // First digit
                monthdelta = (10 * (key - Qt::Key_0)) ;
                if (monthdelta>12) monthdelta=-1 ;
            } else {
                // Second digit
                monthdelta += (key - Qt::Key_0) ;
                if (monthdelta>=1 && monthdelta<=12) {
                    int month = newtime.date().month() ;
                    newtime = newtime.addMonths(monthdelta - month) ;
                    while (newpos<(dateformat.size()) &&
                            (dateformat.at(newpos)=='M' || !(dateformat.at(newpos).isLetter()))) newpos++ ;
                }
                monthdelta=-1 ;
            }
        }
        // handle direct typing of numbers
        else {

            int n, v, nth ;
            QChar t ;

            // Add/subtract date/time numbers based on keypress and position
            n = key - Qt::Key_0 ;               // Current keypress
            t = dateformat.at(pos) ;            // Current date field type
            if (datetext.at(pos)=='-') v=0 ;    // Current date field value (from text)
            else v=datetext.at(pos).toLatin1() - '0' ;
            nth=1 ;                             // nth character in the date field
            for (int j=pos; j<dateformat.length()-1 && dateformat.at(j+1)==t; j++) {
                nth=nth*10 ;
            }

            switch (t.toLatin1()) {
            case 'y':
                if (partialok && datetime.date().year()<10 && nth==1000 && n>0) {
                    // First digit when the year is ----
                    newtime = newtime.addYears((n*nth)-newtime.date().year()) ;
                } else if (partialok && datetime.date().year()<10) {
                    // Any other digit in the year when it is ----
                    invalid=true ;
                } else {
                    newtime = newtime.addYears((n-v)*nth) ;
                }
                if (newtime.date().year()<0) invalid=true ;
                break ;
            case 'd':
                if (nth==10 && n==3) {
                    newtime = newtime.addDays(30-newtime.date().day()) ;
                } else {
                    newtime = newtime.addDays((n-v)*nth) ;
                }
                if (newtime.date().month() != datetime.date().month()) invalid=true ;
                break ;
            case 'h':
                if (nth==10 && n==2) {
                    newtime = newtime.addSecs((20 - newtime.time().hour())*3600) ;
                } else {
                    newtime = newtime.addSecs((n-v)*nth*3600) ;
                }
                if (newtime.date().day() != datetime.date().day()) invalid=true ;
                break ;
            case 'm':
                newtime = newtime.addSecs((n-v)*nth*60) ;
                if (nth==10 && n>5) invalid=true ;
                break ;
            }

            // Work out the new time, and cursor position
            if (newpos<(dateformat.size())) {
                newpos++ ;
                while (newpos<(dateformat.size()) && !(dateformat.at(newpos).isLetter())) newpos++ ;
            }
            monthdelta=-1 ;
        }

    } else {
        invalid=true ;
    }

    // Change the year from 0001 (empty) to 0004 (no year) if the month/day have changed
    if (partialok && newtime.date().year()==1 && (newtime.date().month()!=1 || newtime.date().day()!=1)) {
        newtime = newtime.addYears(3) ;
    }

    if (partialok && newtime.date().year()<10 && newtime.date().year()!=1 && newtime.date().year()!=4) {
        invalid=true ;
    }

    if (invalid) {
        // beep! invalid input
        play(Disabled) ;
        monthdelta=-1 ;
    } else {
        pos=newpos ;
        datetime = newtime ;
        updateDateText() ;
    }

}


