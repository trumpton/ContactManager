#include "advancedfind.h"
#include "ui_advancedfind.h"
#include "contact.h"

struct{
    Contact::ContactRecord type ;
    bool isflag ;
    char *description ;
} advancedfind_data[] = {
{ Contact::Address, false, "Address" },
{ Contact::Comments, false, "Comments / Notes" },
{ Contact::ID, false, "Contact Manager / Google ID" },
{ Contact::Email, false, "Email Address" },
{ Contact::Names, false, "Names" },
{ Contact::Organisation, false, "Organisation" },
{ Contact::Phone, false, "Phone Number" },
{ Contact::Surname, false, "Surname" },
{ Contact::Voip, false, "Voip Phone" },
{ Contact::ProfileAddress, false, "Google Profile" },
{ Contact::Webaddress, false, "Web Address" },
{ Contact::GroupBusiness, true, "Group: Business" },
{ Contact::GroupClient, true, "Group: Client" },
{ Contact::GroupFamily, true, "Group: Family" },
{ Contact::GroupFriend, true, "Group: Friend" },
{ Contact::GroupOther, true, "Group: Other" },
{ Contact::TextMe, true, "Text Enabled" },
{ Contact::EmailMe, true, "Email Enabled" },
{ Contact::Hidden, true, "Hidden" },
{ Contact::NumberOfRecords, false, "" }} ;

AdvancedFind::AdvancedFind(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AdvancedFind)
{
    ui->setupUi(this);

    ui->comboBox_SearchCategory1->addItem(QString(""), (int)Contact::NumberOfRecords);
    ui->comboBox_SearchCategory2->addItem("", (int)Contact::NumberOfRecords);
    ui->comboBox_SearchFlag1->addItem("", (int)Contact::NumberOfRecords);
    ui->comboBox_SearchFlag2->addItem("", (int)Contact::NumberOfRecords);

    for (int i=0; advancedfind_data[i].type != Contact::NumberOfRecords; i++) {
        if (advancedfind_data[i].isflag) {
            ui->comboBox_SearchFlag1->addItem(advancedfind_data[i].description, advancedfind_data[i].type);
            ui->comboBox_SearchFlag2->addItem(advancedfind_data[i].description, advancedfind_data[i].type);
        } else {
            ui->comboBox_SearchCategory1->addItem(advancedfind_data[i].description, advancedfind_data[i].type);
            ui->comboBox_SearchCategory2->addItem(advancedfind_data[i].description, advancedfind_data[i].type);
        }
    }
}

AdvancedFind::~AdvancedFind()
{
    delete ui;
}

Contact::ContactRecord AdvancedFind::categorySearch1()
{
    return (Contact::ContactRecord)ui->comboBox_SearchCategory1->currentData().toInt() ;
}

Contact::ContactRecord AdvancedFind::categorySearch2()
{
    return (Contact::ContactRecord)ui->comboBox_SearchCategory2->currentData().toInt() ;
}

Contact::ContactRecord AdvancedFind::categoryFlag1()
{
    return (Contact::ContactRecord)ui->comboBox_SearchFlag1->currentData().toInt() ;
}

Contact::ContactRecord AdvancedFind::categoryFlag2()
{
    return (Contact::ContactRecord)ui->comboBox_SearchFlag2->currentData().toInt() ;
}

QString AdvancedFind::searchText1()
{
    return ui->lineEdit_SearchText1->text() ;
}

QString AdvancedFind::searchText2()
{
    return ui->lineEdit_SearchText2->text() ;
}

bool AdvancedFind::flagChecked1()
{
    return ui->radioButton_Flag1Enabled->isChecked() ;
}

bool AdvancedFind::flagChecked2()
{
    return ui->radioButton_Flag2Enabled->isChecked() ;
}

void AdvancedFind::on_buttonBox_accepted()
{
    accept() ;
}

void AdvancedFind::on_buttonBox_rejected()
{
    reject() ;
}

void AdvancedFind::on_comboBox_SearchCategory1_currentIndexChanged(int index)
{
    ui->lineEdit_SearchText1->setEnabled(index!=0) ;
}

void AdvancedFind::on_comboBox_SearchCategory2_currentIndexChanged(int index)
{
    ui->lineEdit_SearchText2->setEnabled(index!=0) ;
}


void AdvancedFind::on_comboBox_SearchFlag1_currentIndexChanged(int index)
{
    ui->frame_flags1->setEnabled(index!=0);
}

void AdvancedFind::on_comboBox_SearchFlag2_currentIndexChanged(int index)
{
    ui->frame_flags2->setEnabled(index!=0);
}
