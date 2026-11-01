#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>
#include <QHash>

#include "contact.h"
#include "../Lib/encryption.h"

//using namespace std;

// A detail that differs between the two entries being merged.  The value from the kept entry and the
// value from the removed entry are given, and blank where that entry has none.  When a value has no free
// slot, overflow is set and the value goes to Comments.
struct MergeConflict {
    QString field ;
    QString keptvalue ;
    QString removedvalue ;
    bool overflow ;
} ;

class ContactDatabase
{

public:
    ContactDatabase();
    ~ContactDatabase();

private:
    // Unused copy construct
    ContactDatabase(const ContactDatabase& other) ;
    ContactDatabase& operator =(const ContactDatabase &rhs) ;

public:
    Contact nullitem ;

private:
    QList<Contact> contacts ;
    int selectedcontact ;

    // Shortcuts from a field value to a position in contacts.  They are only
    // shortcuts: see findIndex()
    QHash<QString,int> idindex ;
    QHash<QString,int> gidindex ;
    int findIndex(QHash<QString,int> &index, Contact::ContactRecord type, const QString &value) ;

public:

    bool load() ;
    bool save() ;

    // Moves a deleted contact's files into the deleted folder
    bool archiveDeletedFiles(QString filename) ;

    // Archives deleted contacts that have no Google link.  Deleted contacts that
    // still have a link are counted in linked: a Google sync removes them from
    // Google and clears the link.  Returns the number archived.
    int tidyDeleted(int &linked) ;

    // Removes history lines that repeat an earlier line exactly (same date and text), in every contact.
    // Returns the number of lines removed, and sets changedcontacts to the number of contacts changed.
    int tidyHistories(int &changedcontacts) ;

    // Puts right the email and phone entries that can be, in every contact: a label typed into an entry becomes its title,
    // and the value is tidied.  An entry that cannot be put right is left as it is, and described in unfixed.
    // Returns the number of entries changed, and sets changedcontacts to the number of contacts changed.
    int tidyEntries(int &changedcontacts, QStringList &unfixed) ;
    bool reloadJournal(Contact& contact) ;
    bool saveIndex(int j, QString path, QString contactid, QString name, QString phone, QString email, QString address) ;

    void sort() ;
    int size() ;
    void clear() ;

    Contact& selectContact(int n) ;
    Contact& selectContact(QString id) ;
    Contact& selectContact(Contact &contact) ;
    Contact& getSelected() ;

    Contact& addContact() ;
    bool addContact(Contact &contact, bool dosort=false) ;
    bool deleteContact(int n) ;

    bool isNull() ;

    Contact& getContact(int n) ;
    Contact& getContactBy(Contact::ContactRecord type, QString& data) ;
    Contact& getContactById(QString& searchid) ;
    Contact& getContactByGoogleId(QString& searchid) ;

    // Merging duplicates (works on copies until the merge is accepted)
    bool isMergeCandidate(Contact &a, Contact &b) ;
    bool isSafeToAutoMerge(Contact &a, Contact &b) ;
    void planMerge(Contact &keep, Contact &remove, Contact &merged, QList<MergeConflict> &conflicts) ;
    void applyMerge(Contact &keep, Contact &remove, Contact &merged) ;

    // An unnamed entry has no first name or surname.  tidyUnnamed deletes a blank one that is not on
    // Google, and copies the email address into the surname of one whose only detail is that address.
    // The others are listed in left, with what they hold.
    bool isUnnamed(Contact &c) ;
    void tidyUnnamed(int &emptied, int &renamed, QStringList &left) ;

    int getIndex(Contact& record) ;
    int indexOfContact(QString id) ;      // Position of the contact with this ID, found by search; -1 when there is none
    int getIndex(QString id) ;

};

#endif // DATABASE_H
