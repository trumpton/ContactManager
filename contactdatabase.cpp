#include <QtCore>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QtAlgorithms>
#include <algorithm>
#include <QTextStream>
#include <QElapsedTimer>

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

bool ContactDatabase::archiveDeletedFiles(QString filename)
{
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
    return true ;
}

bool ContactDatabase::load()
{
    QElapsedTimer loadtimer ;
    loadtimer.start() ;

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
            archiveDeletedFiles(filename) ;

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

    dbg(QString("TIMING load contacts: %1 entries, %2 ms").arg(contacts.size()).arg(loadtimer.elapsed())) ;
    return true ;
}

// Archives deleted contacts that have no Google link, and removes them from the list
int ContactDatabase::tidyDeleted(int &linked)
{
    int archived = 0 ;
    linked = 0 ;

    // Save first, so the files match what is in memory
    save() ;

    for (int i=contacts.size()-1; i>=0; i--) {
        Contact &c = contacts[i] ;
        if (!c.isSet(Contact::Deleted)) continue ;

        if (!c.getField(Contact::GoogleRecordId).isEmpty()) {
            linked++ ;
            continue ;
        }

        c.getHistory().addEntry("Moving entry to deleted folder") ;
        addLog("Tidy: moving record " + c.getFormattedName() + " (" + c.getField(Contact::ID) + ") to deleted folder") ;
        archiveDeletedFiles(c.getField(Contact::ID)) ;
        contacts.removeAt(i) ;
        archived++ ;
    }

    selectedcontact = -1 ;
    return archived ;
}

//=====================================================================================================
//
// Merging duplicate contacts
//
//  Two entries are merged without asking only when they agree.  Where a field that holds one value
//  differs, the newer value is kept and the older one is noted in Comments, so nothing is lost.
//  Phone numbers and email addresses use the spare slots instead, so both are kept.
//

// Compare two strings, ignoring case and spaces at the ends
static bool sameText(const QString &a, const QString &b)
{
    return a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive)==0 ;
}

// The last nine digits of a phone number, so that the +44 and 0 forms of one number match
static QString phoneKey(const QString &number)
{
    QString digits ;
    for (int i=0; i<number.size(); i++) {
        if (number.at(i).isDigit()) digits += number.at(i) ;
    }
    return digits.right(9) ;
}

static QString emailKey(const QString &address)
{
    return address.trimmed().toLower() ;
}

// Keys for every phone number on a contact (primary and spare slots)
static QStringList phoneKeys(Contact &c)
{
    Contact::ContactRecord numbers[] = { Contact::Phone, Contact::Work, Contact::Mobile,
                                         Contact::Phone2, Contact::Phone3, Contact::Phone4 } ;
    QStringList keys ;
    for (unsigned int i=0; i<sizeof(numbers)/sizeof(numbers[0]); i++) {
        QString key = phoneKey(c.getField(numbers[i])) ;
        if (!key.isEmpty()) keys << key ;
    }
    return keys ;
}

static QStringList emailKeys(Contact &c)
{
    QStringList keys ;
    QString first = emailKey(c.getField(Contact::Email)) ;
    QString second = emailKey(c.getField(Contact::Email2)) ;
    if (!first.isEmpty()) keys << first ;
    if (!second.isEmpty()) keys << second ;
    return keys ;
}

static bool hasIdentifier(Contact &c)
{
    return !phoneKeys(c).isEmpty() || !emailKeys(c).isEmpty() ;
}

static bool sharesIdentifier(Contact &a, Contact &b)
{
    QStringList pa = phoneKeys(a), pb = phoneKeys(b) ;
    QStringList ea = emailKeys(a), eb = emailKeys(b) ;
    for (const QString &k : pa) if (pb.contains(k)) return true ;
    for (const QString &k : ea) if (eb.contains(k)) return true ;
    return false ;
}

static QString fullName(Contact &c)
{
    return (c.getField(Contact::Names).trimmed() + " " + c.getField(Contact::Surname).trimmed()).trimmed() ;
}

// Note a value that could not be kept in its field, so that the information is not lost
static void noteInComments(Contact &merged, const QString &label, const QString &value)
{
    if (value.trimmed().isEmpty()) return ;
    QString comments = merged.getField(Contact::Comments).trimmed() ;
    QString note = QString("Also recorded (%1): %2").arg(label, value.trimmed()) ;
    merged.setField(Contact::Comments, comments.isEmpty() ? note : comments + "\n" + note) ;
}

// Two entries can be considered for merging when they have the same name, or share a phone number or email
bool ContactDatabase::isMergeCandidate(Contact &a, Contact &b)
{
    QString namea = fullName(a) ;
    bool samename = !namea.isEmpty() && sameText(namea, fullName(b)) ;
    return samename || sharesIdentifier(a, b) ;
}

// Merged without asking only when the names match and the entries share a phone number or email, or
// when neither has either (so they are the same entry, with nothing to tell them apart)
bool ContactDatabase::isSafeToAutoMerge(Contact &a, Contact &b)
{
    QString namea = fullName(a) ;
    if (namea.isEmpty() || !sameText(namea, fullName(b))) return false ;
    return sharesIdentifier(a, b) || (!hasIdentifier(a) && !hasIdentifier(b)) ;
}

// Works out the merged entry in 'merged', which starts as a copy of 'keep'.  'conflicts' lists the
// details that could not all be kept, each with a note of what happened to it.  Nothing is changed in
// 'keep' or 'remove'.
void ContactDatabase::planMerge(Contact &keep, Contact &remove, Contact &merged, QStringList &conflicts)
{
    conflicts.clear() ;
    merged = keep ;

    // Copy the dates first, as getDate() returns a shared object
    QDateTime keepdate = keep.getDate(Contact::Updated) ;
    QDateTime removedate = remove.getDate(Contact::Updated) ;
    bool removenewer = (removedate > keepdate) ;

    // Fields that hold one value.  Where both differ, the newer value is kept and the other is noted.
    Contact::ContactRecord singles[] = { Contact::Surname, Contact::Names, Contact::Organisation,
                                         Contact::Address, Contact::Address2, Contact::Webaddress,
                                         Contact::Birthday } ;
    for (unsigned int n=0; n<sizeof(singles)/sizeof(singles[0]); n++) {
        Contact::ContactRecord f = singles[n] ;
        QString a = keep.getField(f) ;
        QString b = remove.getField(f) ;
        if (a.trimmed().isEmpty()) {
            merged.setField(f, b) ;
        } else if (!b.trimmed().isEmpty() && !sameText(a, b)) {
            QString label = QString(keep.contactRecordName(f)) ;
            conflicts << QString("%1: '%2' or '%3'").arg(label, a.trimmed(), b.trimmed()) ;
            merged.setField(f, removenewer ? b : a) ;
            noteInComments(merged, label, removenewer ? a : b) ;
        }
    }

    // Groups and the contact preferences are combined
    Contact::ContactRecord flags[] = { Contact::GroupBusiness, Contact::GroupClient, Contact::GroupFamily,
                                       Contact::GroupFriend, Contact::GroupOther, Contact::EmailMe, Contact::TextMe } ;
    for (unsigned int n=0; n<sizeof(flags)/sizeof(flags[0]); n++) {
        merged.setFlag(flags[n], keep.isSet(flags[n]) || remove.isSet(flags[n])) ;
    }

    // Comments are joined, as they have no single value
    QString ca = merged.getField(Contact::Comments).trimmed() ;
    QString cb = remove.getField(Contact::Comments).trimmed() ;
    if (!cb.isEmpty() && !sameText(ca, cb)) {
        merged.setField(Contact::Comments, ca.isEmpty() ? cb : ca + "\n\n" + cb) ;
    }

    // Phone numbers.  The primary numbers stay in place, and a new number goes in the first free
    // primary slot, or else in a spare Phone2..Phone4 slot.  A number already held is not added twice.
    struct Number {
        QString title ;
        QString number ;
    } ;
    QList<Number> spare ;
    QStringList seen = phoneKeys(keep) ;
    for (int i=0; i<3; i++) {
        QString number = keep.getField((Contact::ContactRecord)(Contact::Phone2+i)) ;
        if (!number.trimmed().isEmpty()) {
            spare << Number{ keep.getField((Contact::ContactRecord)(Contact::Phone2Title+i)), number } ;
        }
    }

    Contact::ContactRecord primaries[] = { Contact::Phone, Contact::Work, Contact::Mobile } ;
    for (unsigned int n=0; n<sizeof(primaries)/sizeof(primaries[0]); n++) {
        Contact::ContactRecord f = primaries[n] ;
        QString number = remove.getField(f) ;
        QString key = phoneKey(number) ;
        if (number.trimmed().isEmpty() || (!key.isEmpty() && seen.contains(key))) continue ;
        if (!key.isEmpty()) seen << key ;
        if (merged.getField(f).trimmed().isEmpty()) {
            merged.setField(f, number) ;
        } else {
            spare << Number{ QString(), number } ;
        }
    }
    for (int i=0; i<3; i++) {
        QString number = remove.getField((Contact::ContactRecord)(Contact::Phone2+i)) ;
        QString key = phoneKey(number) ;
        if (number.trimmed().isEmpty() || (!key.isEmpty() && seen.contains(key))) continue ;
        if (!key.isEmpty()) seen << key ;
        spare << Number{ remove.getField((Contact::ContactRecord)(Contact::Phone2Title+i)), number } ;
    }

    for (int i=0; i<spare.size(); i++) {
        if (i<3) {
            merged.setField((Contact::ContactRecord)(Contact::Phone2+i), spare[i].number) ;
            merged.setField((Contact::ContactRecord)(Contact::Phone2Title+i), spare[i].title) ;
        } else {
            conflicts << QString("Phone number '%1' has no free slot").arg(spare[i].number) ;
            noteInComments(merged, "phone", spare[i].number) ;
        }
    }
    for (int i=spare.size(); i<3; i++) {
        merged.setField((Contact::ContactRecord)(Contact::Phone2+i), "") ;
        merged.setField((Contact::ContactRecord)(Contact::Phone2Title+i), "") ;
    }

    // Email addresses work the same way, with two slots
    QStringList emailseen = emailKeys(keep) ;
    QString removedemails[2] = { remove.getField(Contact::Email), remove.getField(Contact::Email2) } ;
    for (int i=0; i<2; i++) {
        QString address = removedemails[i].trimmed() ;
        QString key = emailKey(address) ;
        if (address.isEmpty() || emailseen.contains(key)) continue ;
        emailseen << key ;
        if (merged.getField(Contact::Email).trimmed().isEmpty()) {
            merged.setField(Contact::Email, address) ;
        } else if (merged.getField(Contact::Email2).trimmed().isEmpty()) {
            merged.setField(Contact::Email2, address) ;
        } else {
            conflicts << QString("Email address '%1' has no free slot").arg(address) ;
            noteInComments(merged, "email", address) ;
        }
    }

    // History: both lists, without repeats, newest first.  Lines with no readable date go last.
    QStringList all = keep.getHistory().getHistory().split("\n", Qt::SkipEmptyParts) ;
    all << remove.getHistory().getHistory().split("\n", Qt::SkipEmptyParts) ;
    QStringList unique ;
    for (const QString &line : all) {
        if (!unique.contains(line)) unique << line ;
    }
    QList<QPair<QDateTime, QString>> entries ;
    for (const QString &line : unique) {
        entries << QPair<QDateTime, QString>(QDateTime::fromString(line.left(11), "dd-MMM-yyyy"), line) ;
    }
    std::stable_sort(entries.begin(), entries.end(),
        [](const QPair<QDateTime, QString> &x, const QPair<QDateTime, QString> &y) {
            if (x.first.isValid()!=y.first.isValid()) return x.first.isValid() ;
            return x.first > y.first ;
        }) ;
    QString historytext ;
    for (const QPair<QDateTime, QString> &entry : entries) {
        historytext += entry.second + "\n" ;
    }
    merged.getHistory().updateHistory(historytext) ;

    // To-do list: joined, unless the two are the same
    QString todoa = merged.getTodo().getText() ;
    QString todob = remove.getTodo().getText() ;
    if (!todob.trimmed().isEmpty() && !sameText(todoa, todob)) {
        QString combined = todoa.trimmed().isEmpty() ? todob : todoa + "\n" + todob ;
        merged.getTodo().setText(combined) ;
    }

    // Google link.  If only the removed entry is linked, its link moves here, so that the Google record
    // is updated rather than deleted and made again.
    if (merged.getField(Contact::GoogleRecordId).trimmed().isEmpty() &&
            !remove.getField(Contact::GoogleRecordId).trimmed().isEmpty()) {
        merged.setField(Contact::GoogleRecordId, remove.getField(Contact::GoogleRecordId)) ;
        merged.setField(Contact::GoogleEtag, remove.getField(Contact::GoogleEtag)) ;
    }

    // The next Google Sync must send these details, whichever side is the master
    merged.setFlag(Contact::MergePending, true) ;
    merged.setDate(Contact::Updated, QDateTime::currentDateTimeUtc()) ;
}

// Stores the merged entry in 'keep', and marks 'remove' as deleted.  A deleted entry that is still on
// Google is removed there by the next Google Sync, so it cannot come back.
void ContactDatabase::applyMerge(Contact &keep, Contact &remove, Contact &merged)
{
    // If the link moved to the merged entry, it must not also be removed from Google
    bool linkmoved = keep.getField(Contact::GoogleRecordId).trimmed().isEmpty() &&
            !remove.getField(Contact::GoogleRecordId).trimmed().isEmpty() ;

    keep = merged ;
    keep.markAsDirty() ;

    remove.getHistory().addEntry("Merged into another entry") ;
    remove.setFlag(Contact::Deleted, true) ;
    if (linkmoved) {
        remove.setField(Contact::GoogleRecordId, "") ;
        remove.setField(Contact::GoogleEtag, "") ;
    }
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
    out.setEncoding(QStringConverter::Utf8) ;

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
    std::sort(contacts.begin(), contacts.end()) ;
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
// Returns the position of a contact whose field equals value, or -1.
// The hash index is a shortcut only.  A hit is checked against the list, and a
// miss falls back to a full search, so a stale index (after a sort, a removal or
// a change to a field) cannot give a wrong answer.  The guarantee is exact when
// the value is unique, which it always is for contact and Google IDs.  If two
// contacts ever share an ID, the result may be either of them, not necessarily
// the first.
int ContactDatabase::findIndex(QHash<QString,int> &index, Contact::ContactRecord type, const QString &value)
{
    int i = index.value(value, -1) ;
    if (i>=0 && i<contacts.size() && contacts[i].getField(type).compare(value)==0) {
        return i ;
    }

    for (int j=0, ne=contacts.size(); j<ne; j++) {
        if (contacts[j].getField(type).compare(value)==0) {
            index.insert(value, j) ;
            return j ;
        }
    }
    return -1 ;
}

int ContactDatabase::getIndex(QString searchid)
{
    return findIndex(idindex, Contact::ID, searchid) ;
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
    return getContact(findIndex(gidindex, Contact::GoogleRecordId, searchid)) ;
}


bool ContactDatabase::deleteContact(int n)
{
    if (n<0 || n>=contacts.size()) return false ;
    contacts[n].setFlag(Contact::Deleted, true) ;
    contacts[n].setFlag(Contact::Hidden, true) ;
    sort() ;
    return true ;
}


