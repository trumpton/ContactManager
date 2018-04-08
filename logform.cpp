#include "logform.h"
#include "ui_logform.h"
#include <QString>

LogForm::LogForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogForm)
{
    ui->setupUi(this);
}

LogForm::~LogForm()
{
    delete ui;
}

void LogForm::setText(QString& text)
{
    ui->logTextEdit->setText(text) ;
}
