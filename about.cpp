#include "about.h"
#include "ui_about.h"
#include "version.h"
#include "../Lib/supportfunctions.h"

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    QString text = 
    QString("Contact Manager Release %1.\n").arg(BUILDVERSION) +
    QString("It was built on: %1.\n").arg(buildDate()) +
    QString("\nContact Manager Repository Version: %1.\n").arg(appHash()) +
    QString("Library Repository Version: %1.\n").arg(libVersion()) ;

    ui->setupUi(this);
    ui->aboutinformation->setText(text) ;
    ui->aboutinformation->setFocus() ;
}

About::~About()
{
    delete ui;
}
