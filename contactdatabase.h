#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>
#include <QHash>

#include "contact.h"
#include "../Lib/encryption.h"

//using namespace std;

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
    void planMerge(Contact &keep, Contact &remove, Contact &merged, QStringList &conflicts) ;
    void applyMerge(Contact &keep, Contact &remove, Contact &merged) ;

    int getIndex(Contact& record) ;
    int getIndex(QString id) ;

};

#endif // DATABASE_H
