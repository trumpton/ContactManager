#include <QtCore>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QtAlgorithms>

#include "../Lib/supportfunctions.h"
#include "contactdatabase.h"
#include "configuration.h"

ContactDatabase::ContactDatabase()
{
    nullitem.setNull() ;
    clear() ;
}

ContactDatabase::~ContactDatabase()
{
}

void ContactDatabase::clear()
{
    contacts.clear() ;
    selectedcontact=-1 ;
}

bool ContactDatabase::load()
{
    Encryption *enc = gConf->encryption() ;
    if (!enc) {
        dbg("ContactDatabase::load - call without enc") ;
        return false ;
    }

    QStringList nameFilter;
    //QFileInfoList list ;
    QStringList files ;

    // Login if not already done so
    if (!enc->loggedIn()) {
        enc->login() ;
    }
    if (!enc->loggedIn()) {
        return false ;
    }

    // Load Contacts and Journal Entries
    QDir dir(gContactSavePath) ;
    nameFilter << "*.*contact" ;
    dir.setNameFilters(nameFilter) ;
    files = dir.entryList() ;

    for (int i=files.size()-1; i>=0; i--) {

        QString filename ;
        Contact di ;

        filename = files.at(i) ;
        filename = filename.replace(".contact","").replace(".zcontact","") ;
        di.load(gContactSavePath, filename) ;

        // TODO: Need to remove googlerecordid if entry no longer on google
        if (di.isSet(Contact::Deleted) && di.getField(Contact::GoogleRecordId).isEmpty()) {

            // Archive Deleted
            di.getHistory().addEntry("Moving entry to deleted folder") ;
            addLog("Moving record " + di.getFormattedName() + " (" + di.getField(Contact::ID) + ") to deleted folder") ;
            QString deletedPath = gContactSavePath + "deleted" + QDir::toNativeSeparators("/") ;
            QDir path(deletedPath) ;
            if (!path.exists()) path.mkpath(".") ;
            QFile::remove(deletedPath + filename + ".contact") ;
            QFile::remove(deletedPath + filename + ".todolist") ;
            QFile::remove(deletedPath + filename + ".history") ;
            QFile::rename(gContactSavePath + filename + ".contact", deletedPath + filename + ".contact") ;
            QFile::rename(gContactSavePath + filename + ".todolist", deletedPath + filename + ".todolist") ;
            QFile::rename(gContactSavePath + filename + ".history", deletedPath + filename + ".history") ;
            QFile::remove(deletedPath + filename + ".zcontact") ;
            QFile::remove(deletedPath + filename + ".ztodolist") ;
            QFile::remove(deletedPath + filename + ".zhistory") ;
            QFile::rename(gContactSavePath + filename + ".zcontact", deletedPath + filename + ".zcontact") ;
            QFile::rename(gContactSavePath + filename + ".ztodolist", deletedPath + filename + ".ztodolist") ;
            QFile::rename(gContactSavePath + filename + ".zhistory", deletedPath + filename + ".zhistory") ;

        } else {

            // Mark empty entries as deleted
            if (di.isEmpty()) {

                di.getHistory().addEntry("Marking empty record as 'deleted'") ;
                addLog("Marking empty record (" + di.getField(Contact::ID) + ") as deleted") ;
                di.setFlag(Contact::Deleted, true) ;
                contacts.push_back(di) ;

            } else {

                // Add entry
                addLog("Loading " + di.getFormattedName() + " (" + di.getField(Contact::ID) + ")") ;
                contacts.push_back(di) ;

            }

        }
    }

    return true ;
}

// Reload the journal
bool ContactDatabase::reloadJournal(Contact& contact)
{
    return contact.getHistory().load(gContactSavePath, contact.getField(Contact::ID)) ;
}

// Create a HTML Index File of the contacts
bool ContactDatabase::saveIndex(int j, QString path, QString contactid, QString name, QString phone, QString email, QString address)
{
    QString fullPath = path + QString("../contactdatabase.html") ;
    QFile file(fullPath);

    QFile::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text ;
    if (j>=0) {
        mode |= QIODevice::Append ;
    }

    if (!file.open(mode)) {
            return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8") ;

    if (j<0) {
        out << "<html><body><table border='1'>\n" ;
    }

    out << "<tr>\n" ;
    out << "    <td><a target=\"details\" href=\"" << path << contactid << ".contact\"> ... </a></td>\n";
    out << "    <td>" << name << "</td>\n" ;
    out << "    <td>" << phone << "</td>\n" ;
    out << "    <td>" << email << "</td>\n" ;
    out << "    <td>" << address << "</td>\n" ;
    out << "</tr>\n" ;

    if (j>0) {
        out << "</table></body></html>\n" ;
    }

    file.close() ;

    return true ;
}


bool ContactDatabase::save()
{
    Encryption *enc = gConf->encryption() ;
    if (!enc) {
        dbg("ContactDatabase::save call without enc") ;
        return false ;
    }

    sort() ;
    bool success=true ;
    int sz = size() ;


    if (sz>0) {

        // Login if not already done so
        if (!enc->loggedIn()) {
            enc->login() ;
        }
        if (!enc->loggedIn()) {
            return false ;
        }

        QDir cdir(gSavePath) ;
        cdir.mkpath("contact") ;
        for (int i=0; i<sz; i++) {
            int j ;

            if (i==0) j=-1 ;
            else if (i>=(sz-1)) j=1 ;
            else j=0 ;

            success |= getContact(i).save(gContactSavePath) ;

            if (gConf->debugGoogleEnabled()) {
                saveIndex(j, gContactSavePath, getContact(i).getField(Contact::ID),
                      getContact(i).getFormattedName(false, true),
                      getContact(i).getField(Contact::Phone),
                      getContact(i).getField(Contact::Email),
                      getContact(i).getField(Contact::Address)) ;
            }
        }
    }
    return success ;
}

bool ContactDatabase::isNull()
{
    return contacts.isEmpty() ;
}

//
// Sort
//
// Sorts the database into order
//

void ContactDatabase::sort()
{
    QString id = getContact(selectedcontact).getField(Contact::ID) ;
    qSort(contacts.begin(), contacts.end()) ;
    selectedcontact=getIndex(id) ;
}

Contact& ContactDatabase::selectContact(int n)
{
    if (n<-1 || n>=contacts.size()) selectedcontact=n ;
    else selectedcontact=n ;
    return getSelected() ;
}

Contact& ContactDatabase::selectContact(QString id)
{
    selectedcontact=getIndex(id) ;
    return getSelected() ;
}

Contact& ContactDatabase::selectContact(Contact &contact)
{
    selectedcontact=getIndex(contact) ;
    return getSelected() ;
}

Contact& ContactDatabase::getSelected()
{
    if (selectedcontact<0) {
      return nullitem ;
    } else {
      return contacts[selectedcontact];
    }
}




//
// AddRecord
//
// Adds a new empty record to the end of the
// database and returns a pointer to it.
//
Contact& ContactDatabase::addContact()
{
    Contact record ;
    contacts.push_back(record) ;
    return contacts.back() ;
}

bool ContactDatabase::addContact(Contact &contact, bool dosort)
{
    // Find the record for the contact, and if not found, create / add one
    Contact& record = getContactBy(Contact::ID, contact.getField(Contact::ID)) ;
    if (record.isNull()) {
        // Create a new entry and copy the contents to it
        Contact& newcontact = addContact() ;
        contact.copyTo(newcontact, Contact::mcDetailsGroupProfile|Contact::mcId|Contact::mcGoogleId|Contact::mcEtag|Contact::mcControlFlags) ;
    } else {
        // Update the found record with the contact details
        contact.copyTo(record, Contact::mcDetailsGroupProfile) ;
    }
    // Sort the contacts list
   if (dosort) sort() ;
   return true ;
}

//
// GetIndex
//
// Returns the index of the given record in the database
// This is achieved by looking up the ID, and then searching the
// database for a matching ID.
//
int ContactDatabase::getIndex(Contact &record)
{
    return (getIndex(record.getField(Contact::ID))) ;
}

//
// GetIndexFromID
//
// Searches the database, and returns
// the index of the record with the given id, or
// -1 if the record could not be found
//
int ContactDatabase::getIndex(QString searchid)
{
    for (int i=0, ne=contacts.size(); i<ne; i++) {
        QString& recordid = contacts[i].getField(Contact::ID) ;
        if (searchid.compare(recordid)==0) return i ;
    }
    return -1 ;
}

Contact& ContactDatabase::getContactBy(Contact::ContactRecord type, QString& data)
{
    for (int i=0, ne=contacts.size(); i<ne; i++) {
        Contact &c = contacts[i] ;
        if (c.getField(type).compare(data)==0) {
            return c  ;
        }
    }
    return nullitem ;
}


//
// GetNumRecords
//
// Returns the number of records
// in the database
//
int ContactDatabase::size()
{
    return contacts.size() ;
}

//
// getContact
//
// Returns a pointer to the requested record
// or a pointer to the null record if the
// requested entry does not exist.
//
// To guarantee that a valid record is returned,
// n must be between 0 and size()-1.
//
// If this function is used to get a blank null
// contact record, the record should be copied,
// as the nullcontact object is re-used on the
// next getContact call.
//
Contact& ContactDatabase::getContact(int n)
{
    if (n<0 || n >= contacts.size()) {
      return nullitem ;
    } else {
      return contacts[n];
    }
}

Contact& ContactDatabase::getContactById(QString& searchid)
{
    return getContact(getIndex(searchid)) ;
}


Contact& ContactDatabase::getContactByGoogleId(QString& searchid)
{
    for (int i=0, sz=size(); i<sz; i++) {
        Contact &c = contacts[i] ;
        if (c.getField(Contact::GoogleRecordId).compare(searchid)==0) {
            return c ;
        }
    }
    return nullitem ;
}


bool ContactDatabase::deleteContact(int n)
{
    if (n<0 || n>=contacts.size()) return false ;
    contacts[n].setFlag(Contact::Deleted, true) ;
    contacts[n].setFlag(Contact::Hidden, true) ;
    sort() ;
    return true ;
}


