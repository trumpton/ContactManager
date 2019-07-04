#include "about.h"
#include "ui_about.h"
#include "../Lib/supportfunctions.h"

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    QString text = 
            QString("Contact Manager Version: %1.\n").arg(appHash()) +
            QString("Library Version: %1.\n").arg(libVersion()) +
            QString("Built on: %1.\n").arg(buildDate()) ;

    ui->setupUi(this);
    ui->aboutinformation->setText(text) ;
    ui->aboutinformation->setFocus() ;
}

About::~About()
{
    delete ui;
}
