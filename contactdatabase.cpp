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
//  differs, the visible entry wins over a hidden one, otherwise the newer value is kept.  The value that is
//  not kept is recorded in the history.
//  Phone numbers and email addresses use the spare slots instead, so both are kept.
//

// Text for comparing values: lower case, without accents, and without the spaces at the ends
static QString comparable(const QString &text)
{
    QString result = text.trimmed().toLower() ;
    deAccent(result) ;
    return result ;
}

// Compare two values, ignoring case, accents and spaces at the ends
static bool sameText(const QString &a, const QString &b)
{
    return comparable(a) == comparable(b) ;
}

// A name for matching: letters and digits only, lower case, without accents.  So "O'Brien" and
// "Obrien", or "José" and "Jose", give the same name.
static QString nameKey(const QString &name)
{
    QString lower = name.toLower() ;
    deAccent(lower) ;
    QString key ;
    for (int i=0; i<lower.size(); i++) {
        if (lower.at(i).isLetterOrNumber()) key += lower.at(i) ;
    }
    return key ;
}

// Compare two values, using the name rule for names and the ordinary rule for everything else
static bool sameValue(Contact::ContactRecord field, const QString &a, const QString &b)
{
    if (field==Contact::Names || field==Contact::Surname) return nameKey(a) == nameKey(b) ;
    if (field==Contact::Address || field==Contact::Address2) return nameKey(a) == nameKey(b) ;
    return sameText(a, b) ;
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
    Contact::ContactRecord fields[4] = { Contact::Email, Contact::Email2, Contact::Email3, Contact::Email4 } ;
    QStringList keys ;
    for (int i=0; i<4; i++) {
        QString key = emailKey(c.getField(fields[i])) ;
        if (!key.isEmpty()) keys << key ;
    }
    return keys ;
}

static bool hasIdentifier(Contact &c)
{
    return !phoneKeys(c).isEmpty() || !emailKeys(c).isEmpty() ;
}

// Keys for the addresses on a contact, ignoring case and extra spaces
static QStringList addressKeys(Contact &c)
{
    QStringList keys ;
    QString first = nameKey(c.getField(Contact::Address)) ;
    QString second = nameKey(c.getField(Contact::Address2)) ;
    if (!first.isEmpty()) keys << first ;
    if (!second.isEmpty()) keys << second ;
    return keys ;
}

// True if the two contacts share a phone number, an email address or an address
static bool sharesIdentifier(Contact &a, Contact &b)
{
    QStringList pa = phoneKeys(a), pb = phoneKeys(b) ;
    QStringList ea = emailKeys(a), eb = emailKeys(b) ;
    QStringList aa = addressKeys(a), ab = addressKeys(b) ;
    for (const QString &k : pa) if (pb.contains(k)) return true ;
    for (const QString &k : ea) if (eb.contains(k)) return true ;
    for (const QString &k : aa) if (ab.contains(k)) return true ;
    return false ;
}

// The first name must be given on both, and must be the same (ignoring case, punctuation and accents)
static bool firstNamesMatch(Contact &a, Contact &b)
{
    QString firstname = nameKey(a.getField(Contact::Names)) ;
    return !firstname.isEmpty() && firstname == nameKey(b.getField(Contact::Names)) ;
}

// Names must match: the first name and the surname (ignoring case, punctuation and accents).  A shared
// address or phone number does not make two entries candidates, so a husband and wife at one address
// are never offered.
static bool namesMatch(Contact &a, Contact &b)
{
    return firstNamesMatch(a, b) && nameKey(a.getField(Contact::Surname)) == nameKey(b.getField(Contact::Surname)) ;
}

// The day and month of a birthday, as "mm-dd", so that a birthday with or without a year matches.
// Birthdays are stored as "yyyy-mm-dd" or "-mm-dd".
static QString birthdayKey(Contact &c)
{
    QString birthday = c.getField(Contact::Birthday).trimmed() ;
    return birthday.size()>=5 ? birthday.right(5) : QString() ;
}

// Birthdays match on the day and month, and on the year when both have one.  Two different years
// are different birthdays.
static bool birthdaysMatch(Contact &a, Contact &b)
{
    QString x = a.getField(Contact::Birthday).trimmed() ;
    QString y = b.getField(Contact::Birthday).trimmed() ;
    if (x.isEmpty() || y.isEmpty()) return false ;
    if (x.size()==10 && y.size()==10 && x != y) return false ;
    QString birthday = birthdayKey(a) ;
    return !birthday.isEmpty() && birthday == birthdayKey(b) ;
}

// True if the two contacts share a phone number or an email address (but not an address)
static bool sharesPhoneOrEmail(Contact &a, Contact &b)
{
    QStringList pa = phoneKeys(a), pb = phoneKeys(b) ;
    QStringList ea = emailKeys(a), eb = emailKeys(b) ;
    for (const QString &k : pa) if (pb.contains(k)) return true ;
    for (const QString &k : ea) if (eb.contains(k)) return true ;
    return false ;
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
// An unnamed entry has no names to match on.  Two of them would otherwise match each other, so they are
// never offered for merging.
bool ContactDatabase::isUnnamed(Contact &c)
{
    // Not the formatted name, which shows the email address when there is no name
    return c.getField(Contact::Names).trimmed().isEmpty() && c.getField(Contact::Surname).trimmed().isEmpty() ;
}

// Lists what an entry holds, as field names only, so that the list can be read without showing the details
static QStringList heldDetails(Contact &c)
{
    struct Item { Contact::ContactRecord field ; const char *label ; } ;
    const Item items[] = {
        { Contact::Organisation, "organisation" }, { Contact::Address, "address" }, { Contact::Address2, "address 2" },
        { Contact::Phone, "phone" }, { Contact::Work, "work phone" }, { Contact::Mobile, "mobile" },
        { Contact::Phone2, "phone 2" }, { Contact::Phone3, "phone 3" }, { Contact::Phone4, "phone 4" },
        { Contact::Email, "email" }, { Contact::Email2, "other email" }, { Contact::Email3, "email 3" },
        { Contact::Email4, "email 4" }, { Contact::Webaddress, "website" }, { Contact::Birthday, "birthday" },
        { Contact::Comments, "comments" } } ;

    QStringList held ;
    for (unsigned int n=0; n<sizeof(items)/sizeof(items[0]); n++) {
        if (!c.getField(items[n].field).trimmed().isEmpty()) held << items[n].label ;
    }
    if (!c.getTodo().isEmpty()) held << "to-do" ;
    return held ;
}

void ContactDatabase::tidyUnnamed(int &emptied, int &renamed, QStringList &left)
{
    emptied = 0 ;
    renamed = 0 ;
    left.clear() ;

    Contact::ContactRecord emailfields[4] = { Contact::Email, Contact::Email2, Contact::Email3, Contact::Email4 } ;
    Contact::ContactRecord titlefields[2] = { Contact::Email3Title, Contact::Email4Title } ;

    for (int i=0; i<contacts.size(); i++) {
        Contact &c = contacts[i] ;
        if (c.isSet(Contact::Deleted) || !isUnnamed(c)) continue ;

        // A blank entry that is still on Google is left: the next Google Sync clears it there, once
        // Google's copy has been checked.
        if (c.isBlank()) {
            if (!c.getField(Contact::GoogleRecordId).isEmpty()) {
                left << "empty, still on Google: cleared at the next Google Sync" ;
                continue ;
            }
            c.setFlag(Contact::Deleted, true) ;
            c.getHistory().addEntry("Marking empty record as 'deleted'") ;
            emptied++ ;
            continue ;
        }

        // Exactly one email address, and nothing else: the address becomes the surname
        int count = 0 ;
        QString address ;
        for (int n=0; n<4; n++) {
            QString value = c.getField(emailfields[n]).trimmed() ;
            if (!value.isEmpty()) { count++ ; address = value ; }
        }
        Contact probe = c ;
        for (int n=0; n<4; n++) probe.setField(emailfields[n], "") ;
        for (int n=0; n<2; n++) probe.setField(titlefields[n], "") ;
        if (count==1 && probe.isBlank()) {
            c.setField(Contact::Surname, address) ;
            c.getHistory().addEntry("Surname set from its email address") ;
            renamed++ ;
            continue ;
        }

        QStringList held = heldDetails(c) ;
        left << QString("has %1").arg(held.isEmpty() ? QString("no details, but a history") : held.join(", ")) ;
    }
}

bool ContactDatabase::isMergeCandidate(Contact &a, Contact &b)
{
    if (isUnnamed(a) || isUnnamed(b)) return false ;
    if (namesMatch(a, b)) return true ;

    // A change of surname, for example after marriage: the first name and birthday must match,
    // and so must a phone number or email address.  The surname then shows as a difference.
    return firstNamesMatch(a, b) && birthdaysMatch(a, b) && sharesPhoneOrEmail(a, b) ;
}

// Merged without asking only when the names match and the entries share a phone number or email, or
// when neither has either (so they are the same entry, with nothing to tell them apart)
bool ContactDatabase::isSafeToAutoMerge(Contact &a, Contact &b)
{
    if (!namesMatch(a, b)) return false ;
    return sharesIdentifier(a, b) || (!hasIdentifier(a) && !hasIdentifier(b)) ;
}

// Removes repeated history lines from every contact.  A line is a repeat when an identical line (same date
// and text) is already in that contact's history.  The first copy is kept.
int ContactDatabase::tidyHistories(int &changedcontacts)
{
    int removed = 0 ;
    changedcontacts = 0 ;

    for (int i=0; i<contacts.size(); i++) {
        Contact &c = contacts[i] ;
        QStringList lines = c.getHistory().getHistory().split("\n", Qt::SkipEmptyParts) ;
        QStringList unique ;
        for (const QString &line : lines) {
            if (!unique.contains(line)) unique << line ;
        }
        if (unique.size()==lines.size()) continue ;

        removed += lines.size() - unique.size() ;
        changedcontacts++ ;
        c.getHistory().updateHistory(unique.join("\n") + "\n") ;
    }
    return removed ;
}

// Works out the merged entry in 'merged', which starts as a copy of 'keep'.  'conflicts' lists the
// details that differ, with the value from each entry.  Nothing is changed in 'keep' or 'remove'.
void ContactDatabase::planMerge(Contact &keep, Contact &remove, Contact &merged, QList<MergeConflict> &conflicts)
{
    conflicts.clear() ;
    merged = keep ;

    // Lines for the history of the merged entry: values that were replaced, and are not kept
    QStringList droppedlines ;
    QString today = QDateTime::currentDateTime().toString("dd-MMM-yyyy: ") ;

    // Copy the dates first, as getDate() returns a shared object
    QDateTime keepdate = keep.getDate(Contact::Updated) ;
    QDateTime removedate = remove.getDate(Contact::Updated) ;
    bool removenewer = (removedate > keepdate) ;

    // Where one entry is hidden and the other is not, the visible one wins.  Otherwise the newer one wins.
    bool keephidden = keep.isSet(Contact::Hidden) ;
    bool removehidden = remove.isSet(Contact::Hidden) ;
    bool removewins = (keephidden!=removehidden) ? keephidden : removenewer ;

    // Fields that hold one value.  Where both differ, the winning value is kept, and the other is recorded
    // in the history.
    Contact::ContactRecord singles[] = { Contact::Surname, Contact::Names, Contact::Organisation,
                                         Contact::Address, Contact::Address2, Contact::Webaddress,
                                         Contact::Birthday } ;
    for (unsigned int n=0; n<sizeof(singles)/sizeof(singles[0]); n++) {
        Contact::ContactRecord f = singles[n] ;
        QString a = keep.getField(f) ;
        QString b = remove.getField(f) ;

        // Birthdays on the same day and month match when one has no year (stored as "-mm-dd").
        // The one with the year is kept.  Two different years are still a conflict.
        if (f==Contact::Birthday && !a.trimmed().isEmpty() && !b.trimmed().isEmpty() &&
                birthdaysMatch(keep, remove) && (a.trimmed().startsWith('-') || b.trimmed().startsWith('-'))) {
            if (a.trimmed().startsWith('-')) merged.setField(f, b) ;
            continue ;
        }

        if (a.trimmed().isEmpty()) {
            merged.setField(f, b) ;
        } else if (!b.trimmed().isEmpty() && !sameValue(f, a, b)) {
            QString label = QString(keep.contactRecordName(f)) ;
            QString winner = removewins ? b : a ;
            QString loser = removewins ? a : b ;
            conflicts << MergeConflict{ label, a.trimmed(), b.trimmed(), false } ;
            merged.setField(f, winner) ;
            droppedlines << today + QString("Merged: %1 '%2' not kept, '%3' kept").arg(label, loser.trimmed(), winner.trimmed()) + "." ;
        }
    }

    // Groups and the contact preferences are combined
    Contact::ContactRecord flags[] = { Contact::GroupBusiness, Contact::GroupClient, Contact::GroupFamily,
                                       Contact::GroupFriend, Contact::GroupOther, Contact::EmailMe, Contact::TextMe } ;
    for (unsigned int n=0; n<sizeof(flags)/sizeof(flags[0]); n++) {
        merged.setFlag(flags[n], keep.isSet(flags[n]) || remove.isSet(flags[n])) ;
    }

    // Comments are appended, not replaced, unless the two are the same
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
        bool fromremoved ;
    } ;
    QList<Number> spare ;
    QStringList seen = phoneKeys(keep) ;
    for (int i=0; i<3; i++) {
        QString number = keep.getField((Contact::ContactRecord)(Contact::Phone2+i)) ;
        if (!number.trimmed().isEmpty()) {
            spare << Number{ keep.getField((Contact::ContactRecord)(Contact::Phone2Title+i)), number, false } ;
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
            spare << Number{ QString(), number, true } ;
        }
    }
    for (int i=0; i<3; i++) {
        QString number = remove.getField((Contact::ContactRecord)(Contact::Phone2+i)) ;
        QString key = phoneKey(number) ;
        if (number.trimmed().isEmpty() || (!key.isEmpty() && seen.contains(key))) continue ;
        if (!key.isEmpty()) seen << key ;
        spare << Number{ remove.getField((Contact::ContactRecord)(Contact::Phone2Title+i)), number, true } ;
    }

    for (int i=0; i<spare.size(); i++) {
        if (i<3) {
            merged.setField((Contact::ContactRecord)(Contact::Phone2+i), spare[i].number) ;
            merged.setField((Contact::ContactRecord)(Contact::Phone2Title+i), spare[i].title) ;
        } else {
            conflicts << MergeConflict{ "phone",
                spare[i].fromremoved ? QString() : spare[i].number,
                spare[i].fromremoved ? spare[i].number : QString(), true } ;
            noteInComments(merged, "phone", spare[i].number) ;
        }
    }
    for (int i=spare.size(); i<3; i++) {
        merged.setField((Contact::ContactRecord)(Contact::Phone2+i), "") ;
        merged.setField((Contact::ContactRecord)(Contact::Phone2Title+i), "") ;
    }

    // Email addresses: four slots.  Email and Other have fixed labels, and Email3 and Email4 have their
    // own.  Every address is kept.  An address stays in its own slot when that slot is free; otherwise it
    // takes the first free Email3 or Email4 slot.  An address with no slot left goes to Comments.
    Contact::ContactRecord emailfields[4] = { Contact::Email, Contact::Email2, Contact::Email3, Contact::Email4 } ;
    Contact::ContactRecord titlefields[4] = { Contact::Email, Contact::Email2, Contact::Email3Title, Contact::Email4Title } ;
    QString fixedlabels[2] = { "Email", "Other" } ;

    QString addresses[4], titles[4] ;
    QStringList emailseen ;
    for (int i=0; i<4; i++) {
        addresses[i] = keep.getField(emailfields[i]).trimmed() ;
        titles[i] = (i>=2) ? keep.getField(titlefields[i]).trimmed() : QString() ;
        if (!addresses[i].isEmpty()) emailseen << emailKey(addresses[i]) ;
    }
    for (int i=0; i<4; i++) {
        QString address = remove.getField(emailfields[i]).trimmed() ;
        if (address.isEmpty() || emailseen.contains(emailKey(address))) continue ;
        emailseen << emailKey(address) ;

        QString label = (i<2) ? fixedlabels[i] : remove.getField(titlefields[i]).trimmed() ;
        // Try its own slot first.  An address with a fixed label may use either fixed slot before the
        // labelled ones.  A labelled address may use its own slot, then the free labelled slots.
        int candidates[4] = { i, (i<2) ? 1-i : 2, (i<2) ? 2 : 3, 3 } ;
        int slot = -1 ;
        for (int c=0; c<4 && slot<0; c++) {
            if (addresses[candidates[c]].isEmpty()) slot = candidates[c] ;
        }

        if (slot<0) {
            conflicts << MergeConflict{ "email", QString(), address, true } ;
            noteInComments(merged, "email", address) ;
        } else {
            addresses[slot] = address ;
            if (slot>=2) titles[slot] = label.isEmpty() ? QString("Other") : label ;
        }
    }
    for (int i=0; i<4; i++) {
        merged.setField(emailfields[i], addresses[i]) ;
        if (i>=2) merged.setField(titlefields[i], addresses[i].isEmpty() ? QString("") : titles[i]) ;
    }

    // History: both lists, the lines for replaced values, without repeats, newest first.  Lines with
    // no readable date go last.
    QStringList all = keep.getHistory().getHistory().split("\n", Qt::SkipEmptyParts) ;
    all << remove.getHistory().getHistory().split("\n", Qt::SkipEmptyParts) ;
    all << droppedlines ;
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
    remove.setField(Contact::MergedInto, keep.getField(Contact::ID)) ;
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


