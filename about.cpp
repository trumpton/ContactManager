#include "about.h"
#include "ui_about.h"
#include "version.h"

#define TEXT "This is version " BUILDVERSION " (" BUILDDATE ") of Contact Manager, and was built on :\n  " COMPILEDATE

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
