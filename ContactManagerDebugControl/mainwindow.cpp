#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    settings = new QSettings("trumpton.org.uk", "ConfigurationManager");
    ui->debugcontactsxml->setText(settings->value("debugcontactsxml").toString()) ;
    ui->debuggroupsxml->setText(settings->value("debuggroupsxml").toString()) ;
    ui->debugupdatedxml->setText(settings->value("debugupdatedxml").toString()) ;
    ui->debugupdatedxmlresponse->setText(settings->value("debugupdatedxmlresponse").toString()) ;

}

MainWindow::~MainWindow()
{
    delete settings ;
    delete ui;
}

void MainWindow::on_action_Save_triggered()
{
    settings->setValue("debugcontactsxml", ui->debugcontactsxml->text()) ;
    settings->setValue("debuggroupsxml", ui->debuggroupsxml->text()) ;
    settings->setValue("debugupdatedxml", ui->debugupdatedxml->text()) ;
    settings->setValue("debugupdatedxmlresponse", ui->debugupdatedxmlresponse->text()) ;
}

void MainWindow::on_actionE_xit_triggered()
{
    // TODO: find a way to exit properly, i.e. one which calls ~MainWindow
    exit(1) ;
}
