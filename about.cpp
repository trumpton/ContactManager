#include "about.h"
#include "ui_about.h"
#include "../Lib/supportfunctions.h"

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    QString text ;

    if (QString(GITHASH).length()>0) text = text + QString("Contact Manager Version: %1.\n").arg(GITHASH) ;
    if (QString(LIBHASH).length()>0) text = text + QString("Library Version: %1.\n").arg(LIBHASH) ;
    text = text + QString("Build Date: %1.\n").arg(BUILDDATE) ;

    ui->setupUi(this);
    ui->aboutinformation->setText(text) ;
    ui->aboutinformation->setFocus() ;
}

About::~About()
{
    delete ui;
}
