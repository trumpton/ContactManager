
#include "mainwindow.h"
#include "../Lib/alertsound.h"

//
// Navigate Contact Details
//
void MainWindow::on_action_Name_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        errorOkDialog(this, "Debug", "on_action_Name_triggered") ;
        return ;
    } else {
        ui->editFirstName->setFocus() ;
    }
}

void MainWindow::on_actionE_Mail_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        errorOkDialog(this, "Debug", "on_action_E_Mail_triggered") ;
        return ;
    } else {
        ui->editEmail->setFocus() ;
    }
}

void MainWindow::on_action_Address_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        errorOkDialog(this, "Debug", "on_action_Address_triggered") ;
        return ;
    } else {
        ui->editAddress->setFocus() ;
    }
}

void MainWindow::on_action_Phone_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        errorOkDialog(this, "Debug", "on_action_Phone_triggered") ;
        return ;
    } else {
        ui->editHome->setFocus() ;
    }
}

void MainWindow::on_action_Birthday_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        errorOkDialog(this, "Debug", "on_action_Birthday_triggered") ;
        return ;
    } else {
        ui->editDateOfBirth->setFocus() ;
    }
}


//
// on_actionEnableContactDetailsEdit_triggered
//
// Triggered on Alt-E, C (Enable contact details editing)
//
void MainWindow::on_actionEnableContactDetailsEdit_triggered()
{
    if (db.getSelected().isNull()) {
        play(Disabled) ;
        return ;
    } else {
        ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, true);
        ui->tabBar->setCurrentIndex(CONTACTDETAILSTABPOS);
        ui->editFirstName->setFocus() ;
    }
}


void MainWindow::on_actionDeleteContact_triggered()
{
    Contact &dr = db.getSelected() ;
    if (dr.isNull()) {
        play(Disabled) ;
    } else if (dr.getField(Contact::ID).compare(gConf->getMe())==0) {
        warningOkDialog(this, "Error", "You are trying to delete yourself, this is forbidden.\nPlease select a different contact (F5).") ;
    } else {
        if (warningYesNoDialog(this, "Delete Contact?", "Are you sure you want to delete " +
                               dr.getFormattedName() +
                               "\n. This is not easily undone....")) {
            dr.setFlag(Contact::Deleted, true) ;
            dr.setFlag(Contact::Hidden, true) ;

            // Select a blank entry, and refresh the screen / menus
            db.selectContact(SELECT_OVERVIEW);
            populateDialog(SELECT_OVERVIEW) ;            
        }
        play(FileSave) ;
    }
}



void MainWindow::LoadContactTab()
{
    Contact& dr = db.getSelected() ;
    ui->editSurname->setText(dr.getField(Contact::Surname)) ;
    ui->editFirstName->setText(dr.getField(Contact::Names)) ;
    ui->editEmail->setText(dr.getField(Contact::Email)) ;
    ui->editEmail2->setText(dr.getField(Contact::Email2)) ;
    ui->editWebAddress->setText(dr.getField(Contact::Webaddress)) ;
    ui->editBusiness->setText(dr.getField(Contact::Organisation)) ;
    ui->editHome->setText(dr.getField(Contact::Phone)) ;
    ui->editHome2->setText(dr.getField(Contact::Phone2)) ;
    ui->editHome2Title->setText(dr.getField(Contact::Phone2Title)) ;
    ui->editMobile->setText(dr.getField(Contact::Mobile)) ;
    ui->editWork->setText(dr.getField(Contact::Work)) ;
    ui->editVoip->setText(dr.getField(Contact::Voip)) ;
    ui->editDateOfBirth->setDateTime(xsDateToDateTime(dr.getField(Contact::Birthday))) ;
    ui->editAddress->setPlainText(dr.getField(Contact::Address)) ;
    ui->editAddress2->setPlainText(dr.getField(Contact::Address2)) ;
    ui->editComment->setPlainText(dr.getField(Contact::Comments)) ;
    bool textme = dr.isSet(Contact::TextMe);
    ui->checkTextMe->setChecked(textme) ;
    bool emailme = dr.isSet(Contact::EmailMe) ;
    ui->checkEmailMe->setChecked(emailme) ;
    ui->checkHidden->setChecked(dr.isSet(Contact::Hidden)) ;
    QString groupName = dr.getField(Contact::Group) ;


//#define gUNKNOWN "Unknown"
//#define gCLIENT "Client"
//#define gBUSINESS "Business"
//#define gFAMILY "Family"
//#define gOTHER "Other"
//#define gFRIEND "Friend"
//#define gREGULAR "Regular Client"
//#define gCOWORKER "Coworker"
//#define hUNKNOWN "unknown"

    static char contactType[][64] = {
        gBUSINESS,
        gCLIENT,
        "Coworkers",
        "Family",
        "Friends",
        "Other",
        gUNKNOWN
        ""
    } ;

    // Populate the list
    if (groupName.isEmpty()) groupName = "Unknown" ;

    ui->comboBoxContactType->clear() ;
    bool found=false ;
    for (int i=0; contactType[i][0]!='\0'; i++) {
        ui->comboBoxContactType->addItem(contactType[i]);
        if (groupName.compare(contactType[i])==0) found=true ;
    }
    if (!found) ui->comboBoxContactType->addItem(groupName) ;

    // Select the current item
    int index=ui->comboBoxContactType->findText(groupName) ;
    if (index>=0) ui->comboBoxContactType->setCurrentIndex(index) ;

}


bool MainWindow::SaveContactTab()
{
    Contact& dr = db.getSelected() ;

    if (dr.isNull()) return true ;

    Contact newdr = dr ;
    QString log="" ;
    bool wasempty = dr.isEmpty() ;

    newdr.setField(Contact::Group,ui->comboBoxContactType->currentText()) ;
    newdr.setField(Contact::Surname, ui->editSurname->text()) ;
    newdr.setField(Contact::Names,ui->editFirstName->text());
    newdr.setField(Contact::Organisation,ui->editBusiness->text()) ;
    newdr.setField(Contact::Address, ui->editAddress->toPlainText()) ;
    newdr.setField(Contact::Address2, ui->editAddress2->toPlainText()) ;
    newdr.setField(Contact::Phone, ui->editHome->text()) ;
    newdr.setField(Contact::Phone2,ui->editHome2->text());
    newdr.setField(Contact::Phone2Title, ui->editHome2Title->text());
    newdr.setField(Contact::Work, ui->editWork->text());
    newdr.setField(Contact::Mobile, ui->editMobile->text()) ;
    newdr.setField(Contact::Voip, ui->editVoip->text()) ;
    newdr.setField(Contact::Email, ui->editEmail->text()) ;
    newdr.setField(Contact::Email2, ui->editEmail2->text()) ;
    newdr.setField(Contact::Webaddress, ui->editWebAddress->text()) ;
    newdr.setField(Contact::Birthday, dateTimeToXsDate(ui->editDateOfBirth->getDateTime()));
    newdr.setField(Contact::Comments, ui->editComment->toPlainText() );
    newdr.setFlag(Contact::Hidden, ui->checkHidden->checkState()!=Qt::Unchecked) ;
    newdr.setFlag(Contact::EmailMe, ui->checkEmailMe->checkState()!=Qt::Unchecked) ;
    newdr.setFlag(Contact::TextMe, ui->checkTextMe->checkState()!=Qt::Unchecked) ;

    if (!newdr.isDirty()) return true ;

    // Check the textme and emailme fields are right for a client
    if ( newdr.getField(Contact::Group).compare(gCLIENT)==0 &&
        !( newdr.isSet(Contact::TextMe) || newdr.isSet(Contact::EmailMe))) {
            if (!warningYesNoDialog(this,
                 "Contact Details Query",
                 "You've identified the contact as a client, but not selected Text or Email reminders.\nIs this right?")) {
                on_actionEnableContactDetailsEdit_triggered() ;
                ui->checkTextMe->setFocus() ;
                return false ;
            }
    }

    // Check the mobile number is really a mobile number (UK)
    QString mobile = newdr.getField(Contact::Mobile) ;
    mobile.replace(" ","") ;
    if (mobile.left(3).compare("+44")==0 && mobile.left(4).compare("+447")!=0) {
        if (!warningYesNoDialog(this,
             "Contact Details Query",
             "You've selected a mobile number that doesn't look like a mobile number\nIs this right?")) {
            on_actionEnableContactDetailsEdit_triggered() ;
            ui->editMobile->setFocus() ;
            return false ;
        }
    }

    // Record all of the changes made
    if (!wasempty) {
        for (int i=Contact::FIRSTRECORD; i<=Contact::LASTRECORD; i++) {
            enum Contact::ContactRecord t = (Contact::ContactRecord)i ;
            if (newdr.getField(t).compare(dr.getField(t))!=0) {
                // If the record is modified, record the changes in the History
               if (newdr.getField(t).isEmpty()) {
                    log = log + "Deleted " + newdr.getContactRecordName(t) +
                            " (was '" + dr.getField(t).replace("\n",", ") + "')\n" ;
                } else {
                    log = log + "Changed " + newdr.getContactRecordName(t)
                            + " from '" + dr.getField(t).replace("\n",", ")
                            + "' to '" + newdr.getField(t).replace("\n",", ") + "'\n" ;
                }
           }
       }
    }

    dr = newdr ;

    if (!log.isEmpty()) {
        log = QString("Contact Detail Changes: ") + log ;
        dr.getHistory().addEntry(log) ;
    }

    return true ;
}

