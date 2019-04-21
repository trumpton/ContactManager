
#include "mainwindow.h"
#include "../Lib/alertsound.h"

//
// Navigate Contact Details
//
void MainWindow::on_action_Name_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        dbg("Wrong Tab Index") ;
        errorOkDialog(this, "Debug", "on_action_Name_triggered") ;
        return ;
    } else {
        dbg("Setting FirstName Focus") ;
        ui->editFirstName->setFocus() ;
    }
}

void MainWindow::on_actionE_Mail_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        dbg("Wrong Tab Index") ;
        errorOkDialog(this, "Debug", "on_action_E_Mail_triggered") ;
        return ;
    } else {
        dbg("Setting Email Focus") ;
        ui->editEmail->setFocus() ;
    }
}

void MainWindow::on_action_Address_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        dbg("Wrong Tab Index") ;
        errorOkDialog(this, "Debug", "on_action_Address_triggered") ;
        return ;
    } else {
        dbg("Setting Address Focus") ;
        ui->editAddress->setFocus() ;
    }
}

void MainWindow::on_action_Phone_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        dbg("Wrong Tab Index") ;
        errorOkDialog(this, "Debug", "on_action_Phone_triggered") ;
        return ;
    } else {
        dbg("Setting Home Focus") ;
        ui->editHome->setFocus() ;
    }
}

void MainWindow::on_action_Birthday_triggered()
{
    if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
        // Safety check
        dbg("Wrong Tab Index") ;
        errorOkDialog(this, "Debug", "on_action_Birthday_triggered") ;
        return ;
    } else {
        dbg("Setting DOB Focus") ;
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
        dbg("No Contact Selected") ;
        play(Disabled) ;
        return ;
    } else {
        dbg("Moving to Contact Details Tab") ;
        ui->tabBar->setTabEnabled(CONTACTDETAILSTABPOS, true);

        if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS)
            ui->tabBar->setCurrentIndex(CONTACTDETAILSTABPOS) ;

        ui->editFirstName->setFocus() ;
    }
}


void MainWindow::on_actionDeleteContact_triggered()
{
    Contact &dr = db.getSelected() ;
    if (dr.isNull()) {
        dbg("No Contact Selected") ;
        play(Disabled) ;
    } else if (dr.getField(Contact::ID).compare(gConf->getMe())==0) {
        dbg("Attempt to Delete me denied") ;
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
            dbg("Contact deleted") ;
        }
        play(FileSave) ;
    }
}



void MainWindow::LoadContactTab()
{

    dbg("Loading Contact Tab") ;

    Contact& dr = db.getSelected() ;
    dbg(QString("Contact = {%1}").arg(dr.getField(Contact::ID))) ;

    dr.sortPhoneNumbers() ;

    ui->editSurname->setText(dr.getField(Contact::Surname)) ;
    ui->editFirstName->setText(dr.getField(Contact::Names)) ;
    ui->editEmail->setText(dr.getField(Contact::Email)) ;
    ui->editEmail2->setText(dr.getField(Contact::Email2)) ;
    ui->editWebAddress->setText(dr.getField(Contact::Webaddress)) ;
    ui->editBusiness->setText(dr.getField(Contact::Organisation)) ;
    ui->editHome->setText(dr.getField(Contact::Phone)) ;

    ui->edit2->setText(dr.getField(Contact::Phone2)) ;

    if (dr.getField(Contact::Phone2Title).isEmpty()) {
        ui->edit2Title->setText("Other") ;
    } else {
        ui->edit2Title->setText(dr.getField(Contact::Phone2Title)) ;
    }

    ui->edit3->setText(dr.getField(Contact::Phone3)) ;

    if (dr.getField(Contact::Phone3Title).isEmpty()) {
        ui->edit3Title->setText("Other") ;
    } else {
        ui->edit3Title->setText(dr.getField(Contact::Phone3Title)) ;
    }

    ui->edit4->setText(dr.getField(Contact::Phone4)) ;

    if (dr.getField(Contact::Phone4Title).isEmpty()) {
        ui->edit4Title->setText("Other") ;
    } else {
        ui->edit4Title->setText(dr.getField(Contact::Phone4Title)) ;
    }

    ui->editMobile->setText(dr.getField(Contact::Mobile)) ;
    ui->editWork->setText(dr.getField(Contact::Work)) ;

    ui->editVoip->setText(dr.getField(Contact::Voip)) ;

    ui->editDateOfBirth->setDateTime(xsDateToDateTime(dr.getField(Contact::Birthday))) ;
    ui->editAddress->setPlainText(dr.getField(Contact::Address).replace(", ","\n").replace(",","\n")) ;
    ui->editAddress2->setPlainText(dr.getField(Contact::Address2).replace(", ","\n").replace(",","\n")) ;
    ui->editComment->setPlainText(dr.getField(Contact::Comments)) ;
    bool textme = dr.isSet(Contact::TextMe);
    ui->checkTextMe->setChecked(textme) ;
    bool emailme = dr.isSet(Contact::EmailMe) ;
    ui->checkEmailMe->setChecked(emailme) ;
    ui->checkHidden->setChecked(dr.isSet(Contact::Hidden)) ;
    ui->checkBusiness->setChecked(dr.isSet(Contact::GroupBusiness)) ;
    ui->checkClient->setChecked(dr.isSet(Contact::GroupClient)) ;
    ui->checkFamily->setChecked(dr.isSet(Contact::GroupFamily)) ;
    ui->checkFriend->setChecked(dr.isSet(Contact::GroupFriend)) ;
    ui->checkOther->setChecked(dr.isSet(Contact::GroupOther)) ;

    dbg("Contact Tab Load Complete") ;
}


bool MainWindow::SaveContactTab()
{
    dbg("Saving Contact Tab") ;

    Contact& dr = db.getSelected() ;

    if (dr.isNull()) return true ;

    Contact newdr ;
    QString log="" ;
    bool wasempty = dr.isEmpty() ;

    newdr.setField(Contact::Surname, ui->editSurname->text()) ;
    newdr.setField(Contact::Names,ui->editFirstName->text());
    newdr.setField(Contact::Organisation,ui->editBusiness->text()) ;
    newdr.setField(Contact::Address, ui->editAddress->toPlainText().replace("\n", ", ")) ;
    newdr.setField(Contact::Address2, ui->editAddress2->toPlainText().replace("\n", ", ")) ;
    newdr.setField(Contact::Phone, ui->editHome->text()) ;
    newdr.setField(Contact::Phone2,ui->edit2->text());
    if (ui->edit2->text().isEmpty()) {
        newdr.setField(Contact::Phone2Title, "");
    } else {
        newdr.setField(Contact::Phone2Title, ui->edit2Title->text());
    }

    newdr.setField(Contact::Phone3,ui->edit3->text());
    if (ui->edit3->text().isEmpty()) {
        newdr.setField(Contact::Phone3Title, "");
    } else {
        newdr.setField(Contact::Phone3Title, ui->edit3Title->text());
    }

    newdr.setField(Contact::Phone4,ui->edit4->text());
    if (ui->edit4->text().isEmpty()) {
        newdr.setField(Contact::Phone4Title, "");
    } else {
        newdr.setField(Contact::Phone4Title, ui->edit4Title->text());
    }

    newdr.setField(Contact::Voip, ui->editVoip->text()) ;

    newdr.setField(Contact::Work, ui->editWork->text());
    newdr.setField(Contact::Mobile, ui->editMobile->text()) ;
    newdr.setField(Contact::Email, ui->editEmail->text()) ;
    newdr.setField(Contact::Email2, ui->editEmail2->text()) ;
    newdr.setField(Contact::Webaddress, ui->editWebAddress->text()) ;
    newdr.setField(Contact::Birthday, dateTimeToXsDate(ui->editDateOfBirth->getDateTime()));
    newdr.setField(Contact::Comments, ui->editComment->toPlainText() );
    newdr.setFlag(Contact::GroupBusiness, ui->checkBusiness->isChecked()) ;
    newdr.setFlag(Contact::GroupClient, ui->checkClient->isChecked()) ;
    newdr.setFlag(Contact::GroupFamily, ui->checkFamily->isChecked()) ;
    newdr.setFlag(Contact::GroupFriend, ui->checkFriend->isChecked()) ;
    newdr.setFlag(Contact::GroupOther, ui->checkOther->isChecked()) ;
    newdr.setFlag(Contact::Hidden, ui->checkHidden->isChecked()) ;
    newdr.setFlag(Contact::EmailMe, ui->checkEmailMe->checkState()!=Qt::Unchecked) ;
    newdr.setFlag(Contact::TextMe, ui->checkTextMe->checkState()!=Qt::Unchecked) ;
    newdr.sortPhoneNumbers() ;

    // If nothing set / changed / updated, return
    if (!newdr.isDirty()) return true ;
    if (dr.matches(newdr, Contact::mcDetailsGroup)) return true ;

    // Check the textme and emailme fields are right for a client
    if ( !dr.isSet(Contact::GroupClient) && newdr.isSet(Contact::GroupClient) && !( newdr.isSet(Contact::TextMe) || newdr.isSet(Contact::EmailMe))) {
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

    if (wasempty) {

        // Add entry for new contact
        log = log + "Created Contact\n" ;

    } else {

        // Record all of the changes made
        for (int i=0; i<Contact::NumberOfRecords; i++) {
            enum Contact::ContactRecord t = (Contact::ContactRecord)i ;
            if (newdr.isContactOfType((Contact::ContactRecord)i, Contact::mcDetailsGroup)) {

                if (newdr.getField(t).compare(dr.getField(t))!=0) {
                    // If the record is modified, record the changes in the History
                   if (newdr.getField(t).isEmpty()) {
                        log = log + "  Deleted " + newdr.contactRecordName(t) +
                                " (was '" + dr.getField(t).replace("\n",", ") + "')\n" ;
                    } else if (dr.getField(t).isEmpty()) {
                       log = log + "  Set " + newdr.contactRecordName(t) +
                               + "' to '" + newdr.getField(t).replace("\n",", ") + "'\n" ;
                    } else {
                        log = log + "  Changed " + newdr.contactRecordName(t)
                                + " from '" + dr.getField(t).replace("\n",", ")
                                + "' to '" + newdr.getField(t).replace("\n",", ") + "'\n" ;
                   }
                }

            }
        }

        if (!log.isEmpty()) {
            log = QString("Contact Detail Changes: ") + log ;
        }
    }

    newdr.copyTo(dr, Contact::mcDetailsGroup) ;

    if (!log.isEmpty()) {
        dr.getHistory().addEntry(log) ;
        ui->editNotes->document()->setPlainText(dr.getHistory().getHistory());
    }

    dbg("Contact Tab Save Complete") ;

    return true ;
}

