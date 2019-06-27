
#include "mainwindow.h"
#include "../Lib/alertsound.h"
#include "../Lib/supportfunctions.h"
#include "advancedfind.h"

// SEARCH MENU


void MainWindow::on_actionFind_triggered()
{
    bool ok ;
    searchtext = inputDialog(this, tr("Find"),  tr("Search For"), QLineEdit::Normal, searchtext, &ok) ;
    if (ok) on_actionFindNext_triggered() ;
}

void MainWindow::on_actionFindNext_triggered()
{
    bool found=false ;

    if (!searchtext.isEmpty()) {

        QTextEdit *currenttextedit=NULL ;
        QPlainTextEdit *currentplaintextedit=NULL ;
        QListView *currentlistview=NULL;
        AccessibleStringListModel *currentlist=NULL ;

        // Work out what is the  current text field
        switch (ui->tabBar->currentIndex()) {
        case OVERVIEWTABPOS:
            currenttextedit = ui->editOverview ;
            break ;
        case CALENDARTABPOS:
            currentlistview = ui->listCalendar ;
            currentlist = &calendarlist ;
            break ;
        case TODOTABPOS:
            currentplaintextedit = ui->editToDo ;
            break ;
        case HISTORYTABPOS:
            currentplaintextedit = ui->editHistory ;
            break ;
        default:
              ;;
        }

        if (currenttextedit!=NULL) {

            QTextCursor currentpos = currenttextedit->textCursor();
            QTextCursor cursor = currentpos ;
            bool found ;

            // Move cursor on one character
            cursor.movePosition(QTextCursor::NextCharacter) ;
            currenttextedit->setTextCursor(cursor);

            // Search the current textedit field
            if (currenttextedit->find(searchtext)) {
                play(Found) ;
                found=true ;
            } else {
                // Not found, so wrap and try again
                QTextCursor top = currenttextedit->textCursor();
                top.movePosition(QTextCursor::Start) ;
                currenttextedit->setTextCursor(top);
                if (currenttextedit->find(searchtext)) {
                    play(Wrapped) ;
                    found=true ;
                } else {
                    play(NotFound) ;
                    found = false ;
                }
            }

            if (found) {
                // Move cursor to start of selection
                cursor = currenttextedit->textCursor() ;
                int pos = cursor.selectionStart() ;
                cursor.clearSelection() ;
                cursor.setPosition(pos) ;
                currenttextedit->setTextCursor(cursor) ;
                currenttextedit->ensureCursorVisible() ;
            } else {
                // Move cursor back to where it started
                currenttextedit->setTextCursor(currentpos) ;
            }


        }

        if (currentplaintextedit!=NULL) {

            QTextCursor currentpos = currentplaintextedit->textCursor();
            QTextCursor cursor = currentpos ;
            bool found ;

            // Move cursor on one character
            cursor.movePosition(QTextCursor::NextCharacter) ;
            currentplaintextedit->setTextCursor(cursor);

            // Search the current plaintextedit field
            if (currentplaintextedit->find(searchtext)) {
                play(Found) ;
                found = true ;
            } else {
                // Not found, so wrap and try again
                QTextCursor top = currentplaintextedit->textCursor();
                top.movePosition(QTextCursor::Start) ;
                currentplaintextedit->setTextCursor(top);
                if (currentplaintextedit->find(searchtext)) {
                    play(Wrapped) ;
                    found = true ;
                } else {
                    play(NotFound) ;
                    found = false ;
                }
            }

            if (found) {
                // Move cursor to start of selection
                cursor = currentplaintextedit->textCursor() ;
                int pos = cursor.selectionStart() ;
                cursor.clearSelection() ;
                cursor.setPosition(pos) ;
                currentplaintextedit->setTextCursor(cursor) ;
                currentplaintextedit->ensureCursorVisible() ;
            } else {
                // Move cursor back to where it started
                currentplaintextedit->setTextCursor(currentpos) ;
            }

        }

        if (currentlistview!=NULL && currentlist!=NULL) {
            int row = currentlistview->currentIndex().row() ;
            QModelIndex index=currentlist->find(searchtext, row) ;
            if (index.isValid()) {
                play(Found) ;
                found = true ;
            } else {
                index=currentlist->find(searchtext, 0) ;
                if (index.row()>=0) {
                    play(Wrapped) ;
                    found = true ;
                } else {
                    play(NotFound) ;
                    found = false ;
                }
            }

            if (found) {
                dbg("currentlistview->setCurrentIndex()") ;
                currentlistview->setCurrentIndex(index) ;
            }
        }
    }
}

void MainWindow::on_actionGlobalFind_triggered()
{
    bool ok ;

    searchtext = inputDialog(this, tr("Global Find"),  tr("Search For"), QLineEdit::Normal, searchtext, &ok) ;

    if (ok && !searchtext.isEmpty()) {

        class Search *srch ;
        srch = new Search(this) ;

        int numcontacts ;
        int matches = 0 ;

        // Search the calendar first
        if (calendar.find(searchtext, true)>=0) {
            srch->addString("Calendar (future appointments)", "--calendar--") ;
            matches++ ;
        }

        // Then search the non-hidden contact name, journal, but not the history
        numcontacts = db.size() ;
        for (int i=0; i<numcontacts; i++) {
          Contact &contact = db.getContact(i) ;
          if (!contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden)) {
              Todo &todo = contact.getTodo() ;
              if (contact.find(searchtext)>=0 || todo.find(searchtext)>=0) {
                  QString ContactName = contact.getFormattedName(true, true) ;
                  QString id = contact.getField(Contact::ID) ;
                  srch->addString(ContactName, id) ;
                  matches++ ;
              }
          }
        }

        // Finally search the hidden contact name, and journal
        numcontacts = db.size() ;
        for (int i=0; i<numcontacts; i++) {
          Contact &contact = db.getContact(i) ;
          if (!contact.isSet(Contact::Deleted) && contact.isSet(Contact::Hidden)) {
              Todo &todo = contact.getTodo() ;
              if (contact.find(searchtext)>=0 || todo.find(searchtext)>=0) {
                  QString ContactName = "{" + contact.getFormattedName(true, true) + "}";
                  QString id = contact.getField(Contact::ID) ;
                  srch->addString(ContactName, id) ;
                  matches++ ;
              }
          }
        }

        QString id="" ;

        if (matches==0) {
              warningOkDialog(this, "", "No Match Found.") ;
        }

        else if (matches==1) {
            // One active entry, so Skip search dialog
            id = srch->getFirst() ;
            Contact c = db.getContactById(id) ;
            if (!c.isSet(Contact::Deleted) && !c.isSet(Contact::Hidden)) matches=-1 ;
        }

        else if (matches>1) {
            // Several entries, so prompt for choice
             if (srch->exec()==QDialog::Accepted) id = srch->getSelection() ;
        }

        if (id.compare("--calendar--")==0) {
            // Switch to the calendar
            dbg("tabBar->setCurrentIndex(CALEDARTABPOS)") ;
            ui->tabBar->setCurrentIndex(CALENDARTABPOS) ;
            // TODO: search calendarlist, and select returned index
            //ui->listCalendar->set;

        } else if (id.compare("")!=0) {
            // Switch the contact
            populateDialog(id) ;
            ui->editOverview->find(searchtext) ;
            ;;
        }

        delete srch ;
    }

}

bool MainWindow::searchContactRecordSet(QString searchtext, Contact &contact, Contact::ContactRecord record)
{
    bool match=false ;
    bool allempty=true ;

    Contact::ContactRecord r[8] = { Contact::NumberOfRecords, Contact::NumberOfRecords, Contact::NumberOfRecords, Contact::NumberOfRecords,
                                    Contact::NumberOfRecords, Contact::NumberOfRecords, Contact::NumberOfRecords, Contact::NumberOfRecords } ;

    r[0] = record ;
    switch (record) {
    case Contact::Names: r[1]=Contact::ProfileNames ; break ;
    case Contact::Surname: r[1]=Contact::ProfileSurname ; break ;
    case Contact::Address: r[1]=Contact::Address2 ; r[2]=Contact::ProfileAddress ; break ;
    case Contact::Email: r[1]=Contact::Email2 ; r[2]=Contact::ProfileEMail ; break ;
    case Contact::Phone: r[1]=Contact::Work ; r[2]=Contact::Mobile ; r[3]=Contact::Phone2 ; r[4]=Contact::Phone3 ; r[5]=Contact::Phone4 ; r[6]=Contact::ProfilePhone ; break ;
    case Contact::Organisation: r[1]=Contact::ProfileOrg ; break ;
    case Contact::ID: r[1]=Contact::GoogleRecordId ; break ;
    case Contact::ProfileAddress: r[1]=Contact::ProfileEMail ; r[2]=Contact::ProfileNames ; r[3]=Contact::ProfileOrg ; r[4]=Contact::ProfilePhone ; r[5]=Contact::ProfileSurname ; break ;
    default: break ;
    }

    QString srch = searchtext.toLower().replace(" ","") ;
    QRegExp re1(".*(" + srch + ").*") ;
    for (int i=0; i<8; i++) {
        if (r[i]!=Contact::NumberOfRecords) {
            QString fieldtext = contact.getField(r[i]).toLower().replace(" ","") ;
            if (!fieldtext.isEmpty()) allempty=false ;
            if (!srch.isEmpty() && re1.exactMatch(fieldtext)) { match = true ; }
        }
    }
    if (srch.isEmpty() && allempty) match = true ;
    return match ;
}


void MainWindow::on_action_Advanced_Find_triggered()
{
    if (find->exec()==QDialog::Accepted) {

        class Search *srch ;
        srch = new Search(this) ;
        int matches=0 ;
        int numcontacts=db.size() ;

        QString text1 = find->searchText1() ;
        QString text2 = find->searchText2() ;
        bool set1 = find->flagChecked1() ;
        bool set2 = find->flagChecked2() ;

        if (find->categorySearch1()!=Contact::NumberOfRecords ||
            find->categorySearch2()!=Contact::NumberOfRecords ||
            find->categoryFlag1()!=Contact::NumberOfRecords ||
            find->categoryFlag2()!=Contact::NumberOfRecords) {

            for (int i=0; i<numcontacts; i++) {

                Contact &contact = db.getContact(i) ;

                if (!contact.isSet(Contact::Deleted)) {

                    bool match=true ;

                    if (find->categorySearch1()!=Contact::NumberOfRecords) {
                        if (!searchContactRecordSet(text1, contact, find->categorySearch1())) {
                            match=false ;
                        }
                    }

                    if (match && find->categorySearch2()!=Contact::NumberOfRecords) {
                        if (!searchContactRecordSet(text2, contact, find->categorySearch2())) {
                            match=false ;
                        }
                    }

                    if (match && find->categoryFlag1()!=Contact::NumberOfRecords) {
                        bool fieldset = contact.isSet(find->categoryFlag1()) ;
                        if (! (set1==fieldset) ) {
                            match=false ;
                        }
                    }

                    if (match && find->categoryFlag2()!=Contact::NumberOfRecords) {
                        bool fieldset = contact.isSet(find->categoryFlag2()) ;
                        if (! (set2==fieldset) ) {
                            match=false ;
                        }
                    }

                    if (match) {
                        QString ContactName = contact.getFormattedName(true, true) ;
                        QString id = contact.getField(Contact::ID) ;
                        srch->addString(ContactName, id) ;
                        matches++ ;
                    }
                }
            }
        }


        if (matches==0) {
              warningOkDialog(this, "", "No Matches Found") ;
        } else {
            if (srch->exec()==QDialog::Accepted) {
                QString matchid = srch->getSelection() ;
                if (srch->getFirst().compare("")!=0) {
                    populateDialog(matchid) ;
                }
            }
        }
        delete srch ;
    }
}


void MainWindow::on_actionSearch_EmailSMSToContacts_triggered()
{
    class Search *srch ;
    srch = new Search(this) ;
    int matches=0 ;
    int numcontacts = db.size() ;
    QString id="" ;

    for (int i=0; i<numcontacts; i++) {
      Contact &contact = db.getContact(i) ;
      if ( (!contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden))
            && (contact.isSet(Contact::EmailMe) || contact.isSet(Contact::TextMe))) {
          QString ContactName = contact.getFormattedName(true, true) ;
          QString id = contact.getField(Contact::ID) ;
          srch->addString(ContactName, id) ;
          matches++ ;
      }
    }

    if (matches==0) {
          warningOkDialog(this, "", "No Email-To or SMS-To Contacts Found") ;
    } else {
        if (srch->exec()==QDialog::Accepted) {
            QString matchid = srch->getSelection() ;
            if (srch->getFirst().compare("")!=0) {
                populateDialog(matchid) ;
            }
        }
    }

    delete srch ;
}


void MainWindow::on_actionSearch_EmailSMSToNonClients_triggered()
{
    class Search *srch ;
    srch = new Search(this) ;
    int matches=0 ;
    int numcontacts = db.size() ;
    QString id="" ;

    for (int i=0; i<numcontacts; i++) {
      Contact &contact = db.getContact(i) ;
      if ( (!contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden))
            && (contact.isSet(Contact::EmailMe) || contact.isSet(Contact::TextMe))
           && contact.isSet(Contact::GroupClient)) {
          QString ContactName = contact.getFormattedName(true, true) ;
          QString id = contact.getField(Contact::ID) ;
          srch->addString(ContactName, id) ;
          matches++ ;
      }
    }

    if (matches==0) {
          warningOkDialog(this, "", "No Email-To or SMS-To Non-Client Contacts Found") ;
    } else {
        if (srch->exec()==QDialog::Accepted) {
            QString matchid = srch->getSelection() ;
            if (srch->getFirst().compare("")!=0) {
                populateDialog(matchid) ;
            }
        }
    }

    delete srch ;
}


void MainWindow::on_actionSearch_UncategorisedContacts_triggered()
{
    class Search *srch ;
    srch = new Search(this) ;
    int matches=0 ;
    int numcontacts = db.size() ;
    QString id="" ;

    for (int i=0; i<numcontacts; i++) {
      Contact &contact = db.getContact(i) ;
      if ( !contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden)
               && !contact.isSet(Contact::GroupBusiness) && !contact.isSet(Contact::GroupClient)
               && !contact.isSet(Contact::GroupFamily) && !contact.isSet(Contact::GroupFriend) ) {

          QString ContactName = contact.getFormattedName(true, true) ;
          QString id = contact.getField(Contact::ID) ;
          srch->addString(ContactName, id) ;
          matches++ ;
      }
    }

    if (matches==0) {
          warningOkDialog(this, "", "No Unknown Contacts Found.") ;
    } else {
        if (srch->exec()==QDialog::Accepted) {
            QString matchid = srch->getSelection() ;
            if (srch->getFirst().compare("")!=0) {
                populateDialog(matchid) ;
            }
        }
    }

    delete srch ;
}

void MainWindow::on_actionSearch_DuplicateContacts_triggered()
{
    class Search *srch ;
    srch = new Search(this) ;
    int matches=0 ;
    int numcontacts = db.size() ;
    QString id="" ;

    for (int i=0; i<numcontacts; i++) {
        Contact &contact = db.getContact(i) ;
        if (!contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden)) {
            for (int j=0; j<numcontacts; j++) {
                Contact &othercontact = db.getContact(j) ;
                if (!othercontact.isSet(Contact::Deleted) && !othercontact.isSet(Contact::Hidden)) {

                    QString c1 = contact.getField(Contact::Names) + contact.getField(Contact::Surname) + contact.getField(Contact::Organisation) ;
                    QString c2 = othercontact.getField(Contact::Names) + othercontact.getField(Contact::Surname) + othercontact.getField(Contact::Organisation) ;

                    if (i!=j && !c2.isEmpty() && c1.compare(c2)==0) {
                              QString ContactName = contact.getFormattedName(true, true) ;
                              QString id = contact.getField(Contact::ID) ;
                              srch->addString(ContactName, id) ;
                              matches++ ;
                    }
                }
            }
        }
    }

    if (matches==0) {
          warningOkDialog(this, "", "No Duplicates Found.") ;
    } else {
        if (srch->exec()==QDialog::Accepted) {
            QString matchid = srch->getSelection() ;
            if (srch->getFirst().compare("")!=0) {
                populateDialog(matchid) ;
            }
        }
    }

    delete srch ;
}


void MainWindow::on_actionSearch_HiddenEmailDuplicates_triggered()
{
    class Search *srch ;
    srch = new Search(this) ;
    int matches=0 ;
    int numcontacts = db.size() ;
    QString id="" ;

    for (int i=0; i<numcontacts; i++) {

        Contact &contact = db.getContact(i) ;
        QString em = contact.getField(Contact::Email) ;
        QString em2 = contact.getField(Contact::Email2) ;

        if (!contact.isSet(Contact::Hidden) && !contact.isSet(Contact::GroupBusiness) && !contact.isSet(Contact::GroupClient)
                && !contact.isSet(Contact::GroupFamily) && !contact.isSet(Contact::GroupFriend)
                && !contact.isSet(Contact::Deleted) && em.isEmpty() && !em2.isEmpty()) {

            for (int j=0; j<numcontacts; j++) {

                Contact &othercontact = db.getContact(j) ;
                if (!othercontact.isSet(Contact::Deleted)) {

                    if (i!=j && (othercontact.getField(Contact::Email).compare(em2)==0 ||
                                 othercontact.getField(Contact::Email2).compare(em2)==0)) {

                              QString ContactName = othercontact.getFormattedName(true, true) ;
                              QString id = contact.getField(Contact::ID) ;
                              srch->addString(em2 + " (" + ContactName + ")", id) ;
                              matches++ ;

                    }
                }
            }
        }
    }

    if (matches==0) {
          warningOkDialog(this, "", "No Hidden Email addresses duplicated in other contacts found.") ;
    } else {
        if (srch->exec()==QDialog::Accepted) {
            QString matchid = srch->getSelection() ;
            if (srch->getFirst().compare("")!=0) {
                populateDialog(matchid) ;
            }
        }
    }

    delete srch ;
}
