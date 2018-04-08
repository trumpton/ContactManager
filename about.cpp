#include "about.h"
#include "ui_about.h"
#include "buildinfo.h"

#define TEXT "This version of Contact Manager was built on :\n  " BUILDDATE

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    ui->setupUi(this);

    ui->aboutinformation->setText(TEXT) ;
    ui->aboutinformation->setFocus() ;
}

About::~About()
{
    delete ui;
}
