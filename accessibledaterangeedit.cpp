#include "accessibledaterangeedit.h"
#include <QApplication>
#include "../Lib/alertsound.h"

AccessibleDateRangeEdit::AccessibleDateRangeEdit(QWidget *parent) :
    QLineEdit(parent)
{
    pos=0 ;
    monthdelta=-1 ;
    dateprefix = "ddd " ;
    dateformat =  dateprefix + "dd/MMM/yyyy hh:mm" ;
    format = dateformat ;
    QDateTime now = QDateTime::currentDateTime() ;
    setDateTime(now) ;
    updateDateText() ;
}

//
// set and get the date/time
//
void AccessibleDateRangeEdit::setDateTime(QDateTime newdatetime, int dur)
{
    datetime = newdatetime.toLocalTime() ;
    duration = dur ;
    format = dateformat ;
    if (duration>=0) format = format + "  X" ;
    updateDateText() ;
}

QDateTime& AccessibleDateRangeEdit::getDateTime()
{
    return datetime ;
}

int AccessibleDateRangeEdit::getDuration()
{
    return duration ;
}

// Private
void AccessibleDateRangeEdit::updateDateText()
{
    datetext = datetime.toLocalTime().toString(dateformat) ;
    durationtext = "" ;

    if (duration>=0) {
        if (duration==0) {
            durationtext = "reminder" ;
        } else {
            int d = duration/1440 ;
            if (d>0) {
                durationtext = QString::number(d) + "d" ;
            }

            int h = (duration - d*1440)/60;
            int m = (duration - d*1440 - h*60) ;

            if (h>0 || m>0) {
                if (!durationtext.isEmpty()) durationtext = durationtext + " " ;
                QString hours = QString::number(h) ;
                hours = hours.right(2) ;
                durationtext = durationtext + hours + "h" ;
            }

            if (m>0) {
                QString mins = "0" + QString::number(m) ;
                mins=mins.right(2) ;
                durationtext = durationtext + mins ;
            }
        }
        durationtext = " (" + durationtext + ")" ;
    }

    this->setText(datetext + durationtext ) ;
    this->setCursorPosition(pos) ;
}

void AccessibleDateRangeEdit::focusInEvent(QFocusEvent * event)
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
void AccessibleDateRangeEdit::keyPressEvent(QKeyEvent *event)
{

    Qt::KeyboardModifiers keyMod = QApplication::keyboardModifiers();
    int key = event->key() ;
    bool control = keyMod.testFlag(Qt::ControlModifier) ;
    bool alt = keyMod.testFlag(Qt::AltModifier) ;
    bool shift = keyMod.testFlag(Qt::ShiftModifier) ;

    pos = this->cursorPosition() ;
    if (pos<0) pos=0 ;
    if (pos>=format.size()) pos=format.size()-1 ;

    // Skip over whitespace entries
    QChar ch = format.at(pos) ;
    if (!ch.isLetter()) {
        while (pos>=dateprefix.size() && format.at(pos).isLetter()) pos-- ;
        pos++ ;
    }

    // Move to start of month or duration
    ch = format.at(pos) ;
    if (ch=='X' || ch=='M') {
        while (pos>=dateprefix.size() && format.at(pos)==ch) pos-- ;
        pos++ ;
    }

    QDateTime newtime = datetime ;
    int newpos = pos ;
    int newduration = duration ;

    if (    shift || control || alt ||
            key == Qt::Key_Tab ||
            key == Qt::Key_Control ||
            key == Qt::Key_Alt ||
            key == Qt::Key_Shift ||
            key == Qt::Key_Enter ||
            key == Qt::Key_Return ||
            key == Qt::Key_Escape) {

        // default handler for control signals
        monthdelta=-1 ;
        QLineEdit::keyPressEvent(event);


    } else if(event->key() == Qt::Key_Up) {

        // next value for current field
        QChar ch = format.at(pos) ;
        if (!ch.isNull()) {
            while (pos>0 && !ch.isLetter()) ch=format.at(pos-1) ;
            if (ch == 'd') newtime = datetime.addDays(-1) ;
            else if (ch == 'M') newtime = datetime.addMonths(-1) ;
            else if (ch == 'y') newtime = datetime.addYears(-1) ;
            else if (ch == 'h') newtime = datetime.addSecs(-3600) ;
            else if (ch == 'm') newtime = datetime.addSecs(-60) ;
            else if (ch == 'X') {
                // TODO: Round to nearest 15 minutes
                if (duration < 120) newduration-=15 ;
                else if (duration<300) newduration-=30 ;
                else if (duration<1500) newduration-=60 ;
                else newduration-=1440 ;
                if (newduration<0) newduration=0 ;
            }

        }
        monthdelta=-1 ;

    }
    else if(event->key() == Qt::Key_Down) {

        // previous value for current field
        QChar ch = format.at(newpos) ;
        if (!ch.isNull()) {
            while (newpos>0 && !ch.isLetter()) ch=format.at(newpos-1) ;
            if (ch == 'd') newtime = datetime.addDays(1) ;
            else if (ch == 'M') newtime = datetime.addMonths(1) ;
            else if (ch == 'y') newtime = datetime.addYears(1) ;
            else if (ch == 'h') newtime = datetime.addSecs(3600) ;
            else if (ch == 'm') newtime = datetime.addSecs(60) ;
            else if (ch == 'X') {
                // TODO: Round to nearest 15 minutes
                if (duration < 120) newduration+=15 ;
                else if (duration<300) newduration+=30 ;
                else if (duration<1440) newduration+=60 ;
                else newduration+=1440 ;
            }
        }
        monthdelta=-1 ;

    }
    else if(event->key() == Qt::Key_Left) {

        // move left
        QChar ch = format.at(newpos) ;
        while (newpos>0 && format.at(newpos)==ch) newpos-- ;
        if (newpos<dateprefix.size()) {
            newpos=format.size()-1 ;
        }
        while (newpos>0 && !format.at(newpos).isLetter()) newpos-- ;
        while (newpos>0 && format.at(newpos).isLetter()) newpos-- ;
        if (!format.at(newpos).isLetter()) newpos++ ;
        monthdelta=-1 ;

    }
    else if(event->key() == Qt::Key_Right) {

        // move right
        QChar ch = format.at(newpos) ;
        while (newpos<format.size() && format.at(newpos)==ch) newpos++ ;
        while (newpos<format.size() && !format.at(newpos).isLetter()) newpos++ ;
        if (newpos>=format.size()) { newpos=dateprefix.size() ; }
        monthdelta=-1 ;

    }

    // Handle duration shortcuts
    else if (format.at(pos)=='X')  {

        if (key==Qt::Key_0 || key==Qt::Key_R) { newduration=0 ; }
        else if (key>=Qt::Key_1 && key<=Qt::Key_9) { newduration = 60 * (key - Qt::Key_0) ; }
        else if (key==Qt::Key_D) { newduration=1440 ; }
        else {
            play(Disabled) ;
            monthdelta=-1 ;
        }
    }

    else if (key>=Qt::Key_0 && key<=Qt::Key_9 && pos<(format.size())){

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
                    int month = datetime.date().month() ;
                    newtime = datetime.addMonths(monthdelta - month) ;
                    while (newpos<(dateformat.size()) &&
                            (dateformat.at(newpos)=='M' || !(dateformat.at(newpos).isLetter()))) newpos++ ;
                }
                monthdelta=-1 ;
            }
        }

        // handle direct typing of numbers
        else if (datetext.at(newpos).isDigit()) {

            // Replace the given digit with the keypress
            QChar ch = '0' + key - Qt::Key_0 ;
            datetext.replace(newpos, 1, ch) ;

            // If the entry is the first digit of the day, set second digit to 1.
            if (newpos>=dateprefix.size() &&
                (ch=='0' && dateformat.at(newpos)=='d')
                && (dateformat.at(newpos)) != dateformat.at(newpos-1)) {
                datetext.replace(newpos+1,1,'1') ;
            }

            // If the entry is the first digit of the day or hour, zero the second digit.
            else if (newpos>=dateprefix.size() &&
                (dateformat.at(newpos)=='d' || dateformat.at(newpos)=='h')
                && (dateformat.at(newpos)) != dateformat.at(newpos-1)) {
                datetext.replace(newpos+1,1,'0') ;
            }

            // Work out the new time, and cursor position
            newtime = QDateTime::fromString(datetext.mid(dateprefix.size()), dateformat.mid(dateprefix.size())) ;
            if (newpos<(dateformat.size())) {
                newpos++ ;
                if (newpos<(format.size()) && !(format.at(newpos).isLetter())) newpos++ ;
            }
            monthdelta=-1 ;

        } else {
            // beep! invalid input
            play(Disabled) ;
            monthdelta=-1 ;

        }
    } else {
        // beep! invalid input
        play(Disabled) ;
        monthdelta=-1 ;
    }

    bool haschanged=false ;

    if (newtime.isValid()) {
        if (newtime!=datetime || newduration!=duration) {
            haschanged=true ;
        }

        if (newtime!=datetime || newduration!=duration || pos!=newpos) {
            pos=newpos ;
            datetime = newtime ;
            duration = newduration ;
            updateDateText() ;
        }

    } else {
        // beep! invalid input
        play(Disabled) ;
        monthdelta=-1 ;
    }

    if (haschanged) emit valueChanged(datetime, duration) ;
}



