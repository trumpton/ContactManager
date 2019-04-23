#include "about.h"
#include "ui_about.h"
#include "version.h"
#include "../Lib/version.h"

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    QString text = 
    QString("Contact Manager Version %1 (%2).\n").arg(RELEASEVERSION).arg(RELEASEDATE) +
    QString("It was built on: %1.\n").arg(COMPILEDATE) +
    QString("\nContact Manager %1 Repository Version: %2.\n").arg(RELEASEVERSION).arg(GITHASH) +
    QString("Library %1 Repository Version: %2.\n").arg(LIBVERSION).arg(LIBHASH) ;

    ui->setupUi(this);
    ui->aboutinformation->setText(text) ;
    ui->aboutinformation->setFocus() ;
}

About::~About()
{
    delete ui;
}
