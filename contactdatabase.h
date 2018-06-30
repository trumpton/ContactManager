#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>

#include "contact.h"
#include "../Lib/encryption.h"

using namespace std;

class ContactDatabase
{
private:
    QList<Contact> contacts ;
    int selectedcontact ;
    Encryption *enc ;

public:
    Contact nullitem ;

    ContactDatabase();
    ~ContactDatabase();

    void setEncryption(Encryption *enc) ;
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
