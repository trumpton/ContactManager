#include "logform.h"
#include "ui_logform.h"
#include "../Lib/supportfunctions.h"
#include <QString>
#include <QTimer>
#include <QTextCursor>
#include <QScrollBar>

LogForm::LogForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogForm)
{
    ui->setupUi(this);

    // The log is checked once a second, so that entries made while it is open appear
    timer = new QTimer(this) ;
    connect(timer, &QTimer::timeout, this, &LogForm::refreshLog) ;
    timer->start(1000) ;
}

LogForm::~LogForm()
{
    delete ui;
}

void LogForm::setText(QString& text)
{
    // Plain text, so that a log line with angle brackets in it, as in an address, is shown as it is written
    ui->logTextEdit->setPlainText(text) ;
    shown = text ;

    // The newest entries are at the end
    QTextCursor cursor = ui->logTextEdit->textCursor() ;
    cursor.movePosition(QTextCursor::End) ;
    ui->logTextEdit->setTextCursor(cursor) ;
}

// Shows new entries.  If the reader is at the end, they stay at the end; otherwise the place they were reading is kept.
void LogForm::refreshLog()
{
    QString text = getLog() ;
    if (text.size()==shown.size()) return ;

    int position = ui->logTextEdit->textCursor().position() ;
    bool atend = (position >= shown.size()) ;
    int scroll = ui->logTextEdit->verticalScrollBar()->value() ;

    shown = text ;
    ui->logTextEdit->setPlainText(text) ;

    QTextCursor cursor = ui->logTextEdit->textCursor() ;
    if (atend) {
        cursor.movePosition(QTextCursor::End) ;
        ui->logTextEdit->setTextCursor(cursor) ;
    } else {
        cursor.setPosition(qMin(position, text.size())) ;
        ui->logTextEdit->setTextCursor(cursor) ;
        ui->logTextEdit->verticalScrollBar()->setValue(scroll) ;
    }
}
