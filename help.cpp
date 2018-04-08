#include "help.h"
#include "ui_help.h"
#include "../Lib/alertsound.h"
#include <QLineEdit>
#include <QInputDialog>
#include <QKeyEvent>
#include <QDialog>

Help::Help(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Help)
{
    ui->setupUi(this);
    ui->helpText->setAutoHomeEnabled(true) ;
    ui->helpText->setMouseEnabled(false) ;
    searchtext="" ;
}

Help::~Help()
{
    delete ui;
}


void Help::keyPressEvent(QKeyEvent *event)
{
    int key = event->key() ;
    bool nomodifiers = ((event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier|Qt::AltModifier)) == 0) ;
    bool ctl = ((event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier|Qt::AltModifier)) == Qt::ControlModifier) ;

    if (ctl && key==Qt::Key_F) {
        findFirst() ;
    } else if (nomodifiers && key==Qt::Key_F3) {
        findNext() ;
    } else {
        QDialog::keyPressEvent(event);
    }
}

void Help::findFirst()
{
    bool ok ;
    play(Query) ;
    searchtext = QInputDialog::getText(this, tr("Find"),  tr("Search For"), QLineEdit::Normal, searchtext, &ok) ;
    if (ok) findNext() ;
}

//
// TODO: without selecting
//
void Help::findNext()
{
    if (!searchtext.isEmpty()) {

        QTextCursor currentpos = ui->helpText->textCursor();
        QTextCursor cursor = currentpos ;
        bool found ;

        // Move cursor on one character
        cursor.movePosition(QTextCursor::NextCharacter) ;
        ui->helpText->setTextCursor(cursor);

        if (ui->helpText->find(searchtext)) {
            // Clear Selection
            play(Found) ;
            found = true ;
        } else {
            // Not found, so save cursor, wrap and try again
            QTextCursor top = ui->helpText->textCursor();
            top.movePosition(QTextCursor::Start) ;
            ui->helpText->setTextCursor(top);
            if (ui->helpText->find(searchtext)) {
                // Clear Selection
                play(Wrapped) ;
                found=true ;
            } else {
                play(NotFound) ;
                found = false ;
            }
        }
        if (found) {
            // Move cursor to start of selection
            cursor = ui->helpText->textCursor() ;
            int pos = cursor.selectionStart() ;
            cursor.clearSelection() ;
            cursor.setPosition(pos) ;
            ui->helpText->setTextCursor(cursor) ;
            ui->helpText->ensureCursorVisible() ;
        } else {
            // Move cursor back to where it started
            ui->helpText->setTextCursor(currentpos) ;
        }


    }
}
