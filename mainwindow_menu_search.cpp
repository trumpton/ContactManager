
#include "mainwindow.h"
#include "../Lib/alertsound.h"
#include "../Lib/supportfunctions.h"

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
        ListViewStrings *currentlist=NULL ;

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
            currentplaintextedit = ui->editNotes ;
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
            int index = currentlistview->currentIndex().row() ;
            index=currentlist->find(searchtext, index) ;
            if (index>=0) {
                play(Found) ;
                found = true ;
            } else {
                index=currentlist->find(searchtext, -1) ;
                if (index>=0) {
                    play(Wrapped) ;
                    found = true ;
                } else {
                    play(NotFound) ;
                    found = false ;
                }
            }

            if (found) {
                currentlistview->setCurrentIndex(currentlist->findModelIndex(index)) ;
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

        if (matches==1) {
            // One active entry, so Skip search dialog
            id = srch->getFirst() ;
            Contact c = db.getContactById(id) ;
            if (!c.isSet(Contact::Deleted) && !c.isSet(Contact::Hidden)) matches=-1 ;
        }

        if (matches>0) {
             if (srch->exec()==QDialog::Accepted) id = srch->getSelection() ;
        }

        if (id.compare("--calendar--")==0) {
            // Switch to the calendar
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
        if (srch->exec()==QDialog::Accepted) id = srch->getSelection() ;
        if (srch->getFirst().compare("")!=0) {
            populateDialog(id) ;
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
            && (contact.isSet(Contact::EmailMe) || contact.isSet(Contact::TextMe)) &&
           contact.getField(Contact::Group).compare(gCLIENT)!=0) {
          QString ContactName = contact.getFormattedName(true, true) ;
          QString id = contact.getField(Contact::ID) ;
          srch->addString(ContactName, id) ;
          matches++ ;
      }
    }

    if (matches==0) {
          warningOkDialog(this, "", "No Email-To or SMS-To Non-Client Contacts Found") ;
    } else {
        if (srch->exec()==QDialog::Accepted) id = srch->getSelection() ;
        if (srch->getFirst().compare("")!=0) {
            populateDialog(id) ;
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
      if ( (!contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden))
            && (contact.getField(Contact::Group).compare(gUNKNOWN)==0)) {
          QString ContactName = contact.getFormattedName(true, true) ;
          QString id = contact.getField(Contact::ID) ;
          srch->addString(ContactName, id) ;
          matches++ ;
      }
    }

    if (matches==0) {
          warningOkDialog(this, "", "No Unknown Contacts Found.") ;
    } else {
        if (srch->exec()==QDialog::Accepted) id = srch->getSelection() ;
        if (srch->getFirst().compare("")!=0) {
            populateDialog(id) ;
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
        if (srch->exec()==QDialog::Accepted) id = srch->getSelection() ;
        if (srch->getFirst().compare("")!=0) {
            populateDialog(id) ;
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
        QString gr = contact.getField(Contact::Group) ;
        QString em = contact.getField(Contact::Email) ;
        QString em2 = contact.getField(Contact::Email2) ;

        if (gr.compare(gUNKNOWN)==0 && contact.isSet(Contact::Hidden) && !contact.isSet(Contact::Deleted) && em.isEmpty() && !em2.isEmpty()) {

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
        if (srch->exec()==QDialog::Accepted) id = srch->getSelection() ;
        if (srch->getFirst().compare("")!=0) {
            populateDialog(id) ;
        }
    }

    delete srch ;
}
