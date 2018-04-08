#include "transactionform.h"
#include "ui_transactionform.h"

TransactionForm::TransactionForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionForm)
{
    ui->setupUi(this);
}

TransactionForm::~TransactionForm()
{
    delete ui;
}

void TransactionForm::setupForm(QString formTitle,
       QString transactionNameTitle,
       QString transactionDescriptionTitle,
       QString transactionValueTitle,
       QString defaultName,
       QString defaultTitle,
       QString defaultAmount,
       QString accessibleNameText,
       QString accessibleDescriptionText,
       QString accessibleValueText)
{
    this->setWindowTitle(formTitle) ;
    ui->PersonLabel->setText(transactionNameTitle) ;
    ui->TitleLabel->setText(transactionDescriptionTitle) ;
    ui->AmountLabel->setText(transactionValueTitle) ;
    ui->lineEditPerson->setText(defaultName) ;
    ui->lineEditTitle->setText(defaultTitle) ;
    ui->lineEditAmount->setText(defaultAmount) ;
    ui->lineEditPerson->setAccessibleName(accessibleNameText) ;
    ui->lineEditTitle->setAccessibleName(accessibleDescriptionText) ;
    ui->lineEditAmount->setAccessibleName(accessibleValueText) ;
    ui->lineEditPerson->setFocus() ;
}


QString& TransactionForm::getValue()
{
    static QString res ;
    res=ui->lineEditAmount->text() ;
    return res ;
}

QString& TransactionForm::getDescription()
{
    static QString res ;
    res=ui->lineEditTitle->text() ;
    return res ;
}


