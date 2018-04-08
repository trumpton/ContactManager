#include "sendsmsform.h"
#include "ui_sendsmsform.h"

SendSMSForm::SendSMSForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendSMSForm)
{
    ui->setupUi(this);
    ui->recipientText->setReadOnly(true) ;
}

SendSMSForm::~SendSMSForm()
{
    delete ui;
}

QString SendSMSForm::getMessage()
{
    return ui->messageText->text() ;
}

void SendSMSForm::setRecipient(QString recipient)
{
    ui->recipientText->setText(recipient) ;
}
