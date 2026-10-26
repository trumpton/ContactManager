
#include "mainwindow.h"
#include "fieldformat.h"
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
        dbg("editFirstName->setFocus()") ;
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
        dbg("editEmail->setFocus()") ;
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
        dbg("editAddress->setFocus()") ;
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
        dbg("editHome->setFocus()") ;
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
        dbg("editDateOfBirth->setFocus()") ;
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

        if (ui->tabBar->currentIndex()!=CONTACTDETAILSTABPOS) {
            dbg("tabBar->setCurrentIndex(CALENDARTABPOS)") ;
            ui->tabBar->setCurrentIndex(CONTACTDETAILSTABPOS) ;
        }

        dbg("editFirstName->setFocus()") ;
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



// The label the editor shows for a phone slot, with the same rules as emailLabelForEditor
static QString phoneLabelForEditor(Contact &dr, int slot)
{
    static const Contact::ContactRecord phonefields[6] = { Contact::Phone, Contact::Work, Contact::Mobile, Contact::Phone2, Contact::Phone3, Contact::Phone4 } ;
    if (dr.getField(phonefields[slot]).isEmpty()) return Contact::defaultPhoneLabel(slot) ;
    return dr.phoneLabel(slot) ;
}

// Each email and phone entry is named by its title, and F2 in the entry changes that title.  The title is plain
// text, not a tab stop, so tabbing goes from entry to entry.
struct TitledEntry {
    QLineEdit *entry ;
    QLabel *title ;
    QString kind ;          // "Email" or "Phone", used in the name and the dialog
    int slot ;              // the email or phone slot, for its default label
    Contact::ContactRecord value ;  // the record the entry is saved to
} ;

static QList<TitledEntry> titledEntries(Ui::MainWindow *ui)
{
    return {
        { ui->editEmail, ui->editEmailTitle, QString("Email"), 0, Contact::Email },
        { ui->editEmail2, ui->editEmail2Title, QString("Email"), 1, Contact::Email2 },
        { ui->editEmail3, ui->editEmail3Title, QString("Email"), 2, Contact::Email3 },
        { ui->editEmail4, ui->editEmail4Title, QString("Email"), 3, Contact::Email4 },
        { ui->editHome, ui->editPhoneTitle, QString("Phone"), 0, Contact::Phone },
        { ui->editWork, ui->editWorkTitle, QString("Phone"), 1, Contact::Work },
        { ui->editMobile, ui->editMobileTitle, QString("Phone"), 2, Contact::Mobile },
        { ui->edit2, ui->edit2Title, QString("Phone"), 3, Contact::Phone2 },
        { ui->edit3, ui->edit3Title, QString("Phone"), 4, Contact::Phone3 },
        { ui->edit4, ui->edit4Title, QString("Phone"), 5, Contact::Phone4 },
    } ;
}

void MainWindow::setupTitledEntries()
{
    for (const TitledEntry &t : titledEntries(ui)) t.entry->installEventFilter(this) ;
}

void MainWindow::refreshTitledEntries()
{
    for (const TitledEntry &t : titledEntries(ui)) t.entry->setAccessibleName(t.title->text() + " " + t.kind) ;
}

// F2 in an email or phone entry opens a dialog for that entry's title.  When focus leaves an entry it is tidied.
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    QLineEdit *entry = qobject_cast<QLineEdit *>(watched) ;
    if (entry && event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_F2) {
        changeTitle(entry) ;
        return true ;
    }
    if (entry && event->type() == QEvent::FocusOut) tidyTitledEntry(entry) ;
    return QMainWindow::eventFilter(watched, event) ;
}

// The rules for a title, for the message when one is not allowed
static QString labelRules()
{
    return QString("A title can use letters, numbers, spaces and most symbols.  It cannot contain a colon, a slash, a backslash, "
                   "or a tab or line break.  It can be up to %1 characters long.").arg(FieldFormat::MaxLabelLength) ;
}

// Puts right one email or phone entry that can be: a label typed into it becomes its title, and the value is tidied.
// An entry that cannot be put right is left as it is, and the save check reports it.
void MainWindow::tidyTitledEntry(QLineEdit *entry)
{
    for (const TitledEntry &t : titledEntries(ui)) {
        if (t.entry != entry) continue ;

        QString text = t.entry->text() ;
        FieldFormat::Tidied r = FieldFormat::tidyEntry(text, t.kind=="Email") ;
        if (r.fixed) {
            if (r.value.compare(text)!=0) t.entry->setText(r.value) ;
            if (!r.label.isEmpty()) t.title->setText(r.label) ;
            refreshTitledEntries() ;
        }
        return ;
    }
}

// Run when the form is loaded, so entries from Google and older entries are shown as they should be
void MainWindow::tidyTitledEntries()
{
    for (const TitledEntry &t : titledEntries(ui)) tidyTitledEntry(t.entry) ;
    refreshTitledEntries() ;
}

// Checks every email and phone entry on the form, when the contact is saved, so entries from Google or from older data
// are checked too.  A label typed into an entry is tidied first.  If an entry or its title is not right, the first one is
// shown with focus on it, and the save stops, so nothing is saved and the user returns to editing.  A title is changed
// with F2.  Returns false when an entry is not right.
bool MainWindow::ValidateTitledEntries()
{
    for (const TitledEntry &t : titledEntries(ui)) {
        QString text = t.entry->text() ;
        if (text.trimmed().isEmpty()) continue ;

        FieldFormat::Tidied r = FieldFormat::tidyEntry(text, t.kind=="Email") ;
        if (!r.fixed) {
            QString name = t.title->text() + " " + t.kind.toLower() ;
            QString hint = (t.kind=="Email") ? QString("An email address looks like name@domain.com.")
                : QString("Use digits, spaces, brackets, hyphens and dots, with an optional + at the start.") ;
            warningOkDialog(this, "Contact Details", QString("The %1 \"%2\" is not formatted correctly: %3.\n\n%4")
                            .arg(name, text, r.problem, hint), true) ;
            t.entry->setFocus() ;
            titledEntriesHeld = true ;
            return false ;
        }

        t.entry->setText(r.value) ;
        if (!r.label.isEmpty()) t.title->setText(r.label) ;

        // A title that is not allowed, from Google or older data, is changed with F2 on its entry
        QString title = t.title->text() ;
        if (!title.isEmpty() && !FieldFormat::validLabel(title)) {
            QString name = title + " " + t.kind.toLower() ;
            warningOkDialog(this, "Contact Details", QString("The title \"%1\" for the %2 is not allowed: %3.\n\n"
                            "Press F2 in the %2 to change its title.").arg(title, name, labelRules()), true) ;
            t.entry->setFocus() ;
            titledEntriesHeld = true ;
            return false ;
        }
    }

    titledEntriesHeld = false ;
    refreshTitledEntries() ;
    return true ;
}

// Changes the title of one email or phone for this contact.  A blank title gives the default label back.
void MainWindow::changeTitle(QLineEdit *entry)
{
    for (const TitledEntry &t : titledEntries(ui)) {
        if (t.entry != entry) continue ;

        QString defaultlabel = (t.kind == "Email") ? Contact::defaultEmailLabel(t.slot) : Contact::defaultPhoneLabel(t.slot) ;
        QString windowtitle = QString("Title for this %1").arg(t.kind) ;
        QString prompt = QString("Title for this %1 (blank gives the default, %2):").arg(t.kind.toLower(), defaultlabel) ;

        bool ok = false ;
        QString title = QInputDialog::getText(this, windowtitle, prompt, QLineEdit::Normal, t.title->text(), &ok) ;
        if (!ok) return ;

        title = title.trimmed() ;
        if (title.isEmpty()) {
            title = defaultlabel ;
        } else if (!FieldFormat::validLabel(title)) {
            warningOkDialog(this, windowtitle, QString("\"%1\" is not a valid title.\n\n%2").arg(title, labelRules()), true) ;
            t.entry->setFocus() ;
            return ;
        }

        t.title->setText(FieldFormat::normaliseLabel(title)) ;
        refreshTitledEntries() ;
        return ;
    }
}

// The label the editor shows for an email slot.  An empty slot shows the label a new email gets there, and a slot
// with an address shows its own label, or the label that slot has always had.
static QString emailLabelForEditor(Contact &dr, int slot)
{
    static const Contact::ContactRecord emailfields[4] = { Contact::Email, Contact::Email2, Contact::Email3, Contact::Email4 } ;
    if (dr.getField(emailfields[slot]).isEmpty()) return Contact::defaultEmailLabel(slot) ;
    return dr.emailLabel(slot) ;
}

void MainWindow::LoadContactTab()
{
    // A refused entry is kept as it was typed, so the form is not reloaded until it is fixed
    if (titledEntriesHeld) return ;

    dbg("Loading Contact Tab") ;

    Contact& dr = db.getSelected() ;
    dbg(QString("Contact = {%1}").arg(dr.getField(Contact::ID))) ;

    ui->editSurname->setText(dr.getField(Contact::Surname)) ;
    ui->editFirstName->setText(dr.getField(Contact::Names)) ;
    // A slot with no address shows the label a new email gets there.  A slot with an address shows its own label.
    ui->editEmail->setText(dr.getField(Contact::Email)) ;
    ui->editEmail2->setText(dr.getField(Contact::Email2)) ;
    ui->editEmail3->setText(dr.getField(Contact::Email3)) ;
    ui->editEmail4->setText(dr.getField(Contact::Email4)) ;
    ui->editEmailTitle->setText(emailLabelForEditor(dr, 0)) ;
    ui->editEmail2Title->setText(emailLabelForEditor(dr, 1)) ;
    ui->editEmail3Title->setText(emailLabelForEditor(dr, 2)) ;
    ui->editEmail4Title->setText(emailLabelForEditor(dr, 3)) ;
    ui->editWebAddress->setText(dr.getField(Contact::Webaddress)) ;
    ui->editBusiness->setText(dr.getField(Contact::Organisation)) ;
    ui->editHome->setText(dr.getField(Contact::Phone)) ;
    ui->editWork->setText(dr.getField(Contact::Work)) ;
    ui->editMobile->setText(dr.getField(Contact::Mobile)) ;
    ui->edit2->setText(dr.getField(Contact::Phone2)) ;
    ui->edit3->setText(dr.getField(Contact::Phone3)) ;
    ui->edit4->setText(dr.getField(Contact::Phone4)) ;
    ui->editPhoneTitle->setText(phoneLabelForEditor(dr, 0)) ;
    ui->editWorkTitle->setText(phoneLabelForEditor(dr, 1)) ;
    ui->editMobileTitle->setText(phoneLabelForEditor(dr, 2)) ;
    ui->edit2Title->setText(phoneLabelForEditor(dr, 3)) ;
    ui->edit3Title->setText(phoneLabelForEditor(dr, 4)) ;
    ui->edit4Title->setText(phoneLabelForEditor(dr, 5)) ;
    tidyTitledEntries() ;

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

    // An entry that is not right stops the save, and the user returns to editing
    if (!ValidateTitledEntries()) return false ;

    Contact newdr ;
    QString log="" ;
    bool wasempty = dr.isEmpty() ;

    newdr.setField(Contact::Surname, ui->editSurname->text()) ;
    newdr.setField(Contact::Names,ui->editFirstName->text());
    newdr.setField(Contact::Organisation,ui->editBusiness->text()) ;
    newdr.setField(Contact::Address, ui->editAddress->toPlainText().replace("\n", ", ")) ;
    newdr.setField(Contact::Address2, ui->editAddress2->toPlainText().replace("\n", ", ")) ;
    newdr.setField(Contact::Phone, ui->editHome->text()) ;
    newdr.setField(Contact::Work, ui->editWork->text()) ;
    newdr.setField(Contact::Mobile, ui->editMobile->text()) ;
    newdr.setField(Contact::Phone2, ui->edit2->text()) ;
    newdr.setField(Contact::Phone3, ui->edit3->text()) ;
    newdr.setField(Contact::Phone4, ui->edit4->text()) ;
    newdr.setField(Contact::PhoneTitle, ui->editHome->text().isEmpty() ? QString("") : ui->editPhoneTitle->text()) ;
    newdr.setField(Contact::WorkTitle, ui->editWork->text().isEmpty() ? QString("") : ui->editWorkTitle->text()) ;
    newdr.setField(Contact::MobileTitle, ui->editMobile->text().isEmpty() ? QString("") : ui->editMobileTitle->text()) ;
    newdr.setField(Contact::Phone2Title, ui->edit2->text().isEmpty() ? QString("") : ui->edit2Title->text()) ;
    newdr.setField(Contact::Phone3Title, ui->edit3->text().isEmpty() ? QString("") : ui->edit3Title->text()) ;
    newdr.setField(Contact::Phone4Title, ui->edit4->text().isEmpty() ? QString("") : ui->edit4Title->text()) ;
    newdr.setField(Contact::Email, ui->editEmail->text()) ;
    newdr.setField(Contact::Email2, ui->editEmail2->text()) ;
    newdr.setField(Contact::Email3, ui->editEmail3->text()) ;
    newdr.setField(Contact::Email4, ui->editEmail4->text()) ;
    newdr.setField(Contact::EmailTitle, ui->editEmail->text().isEmpty() ? QString("") : ui->editEmailTitle->text()) ;
    newdr.setField(Contact::Email2Title, ui->editEmail2->text().isEmpty() ? QString("") : ui->editEmail2Title->text()) ;
    newdr.setField(Contact::Email3Title, ui->editEmail3->text().isEmpty() ? QString("") : ui->editEmail3Title->text()) ;
    newdr.setField(Contact::Email4Title, ui->editEmail4->text().isEmpty() ? QString("") : ui->editEmail4Title->text()) ;
    newdr.setField(Contact::Voip, ui->editVoip->text()) ;
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

    // If nothing set / changed / updated, return.  A field that was cleared does not make newdr dirty, so compare with dr.
    if (dr.matches(newdr, Contact::mcDetailsGroup)) return true ;

    // Check the textme and emailme fields are right for a client
    if ( gConf->getEnableReminders() &&
         !dr.isSet(Contact::GroupClient) &&
         newdr.isSet(Contact::GroupClient) &&
         !( newdr.isSet(Contact::TextMe) || newdr.isSet(Contact::EmailMe))) {
            if (!warningYesNoDialog(this,
                 "Contact Details Query",
                 "You've identified the contact as a client, but not selected Text or Email reminders.\nIs this right?")) {
                on_actionEnableContactDetailsEdit_triggered() ;
                dbg("checkTextMe->setFocus()") ;
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
            dbg("editMobile->setFocus()") ;
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

    // Copy details and group, whilst retaining non-form parameters
    newdr.setField(Contact::Deleted, dr.getField(Contact::Deleted)) ;
    newdr.copyTo(dr, Contact::mcDetailsGroup) ;

    if (!log.isEmpty()) {
        appendHistory(dr, log) ;
    }

    // If contact has been deleted, select a blank entry, and refresh the screen / menus
    // This would happen if contact is open, and it is removed by a sync operation
    if (dr.isSet(Contact::Deleted)) {
        db.selectContact(SELECT_OVERVIEW);
        populateDialog(SELECT_OVERVIEW) ;
    }

    dbg("Contact Tab Save Complete") ;

    return true ;
}

