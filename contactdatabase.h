#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>

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

public:

    bool load() ;
    bool save() ;
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

    int getIndex(Contact& record) ;
    int getIndex(QString id) ;

};

#endif // DATABASE_H
