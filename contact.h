#ifndef CUSTOMERRECORD_H
#define CUSTOMERRECORD_H

#include <QString>
#include <QDateTime>
#include "history.h"
#include "todo.h"
#include "../Lib/encryption.h"

// Status
#define stHIDDEN "hidden"
#define stDELETED "deleted"
#define stACTIVE "active"
#define stPURGED "purged"

// Groups
#define gUNKNOWN "Unknown"
#define gBUSINESS "Business"
#define gCLIENT "Client"

class Contact
{
public:
    enum ContactRecord {

        //--------------------------
        // Contact Data

        FIRSTRECORD = 0,

        FIRSTSYNCEDRECORD = 0,      // All data items that are synced - changes to these causes Updated to change

        FIRSTSYNCEDDATA = 0,        // Setting any of these will make isempty=false

        FIRSTDIALOGEDITEDDATA = 0,  // Records edited in the contact dialog

        // These entries are synchronised, and are used to work out
        // if an entry is empty

        Surname = 0,
        Names = 1,
        Organisation = 2,

        STARTADDRESS = 3,
        Address = 3,
        Address2 = 4,
        ENDADDRESS = 4,

        STARTPHONE = 5,
        Phone = 5,
        Phone2 = 6,
        Work = 7,
        Mobile = 8,
        ENDPHONE = 8,

        Voip = 9,

        STARTEMAIL = 10,
        Email = 10,
        Email2 = 11,
        ENDEMAIL = 11,

        Webaddress = 12,
        Birthday = 13,
        Comments = 14,

        LASTSYNCEDDATA = 14,

        // These entries are synchronised, but not used to work out if the
        // entry is empty.

        Phone2Title = 15,
        Group = 16,
        Hidden = 17,            // Hidden entries are not in MyContacts on Google
        EmailMe =  18,
        TextMe = 19,

        LASTDIALOGEDITEDDATA = 19,

        Deleted = 20,
        ID = 21,                // Unique ID

        LASTSYNCEDRECORD = 21,

        // These are local entries and are not uploaded to Google

        Updated = 22,           // Local updated timestamp
        Created = 23,           // Local created timestamp

        GOOGLEFIRSTRECORD = 24,

        // These are Google specific data entries

        GoogleAccount = 24,
        GoogleEtag = 25,
        GoogleRecordId = 26,
        GoogleSequence = 27,
        GoogleCreated = 28,
        GoogleDeleted = 29,     // Set when deleted on google
        GoogleUploaded = 30,    // Set by successful upload process

        GOOGLELASTRECORD = 30,

        LASTRECORD = 30

    };

    enum ContactOverviewType {
        contactAsText = 0,
        contactAsHTML = 1,
        contactAsVCARD = 2
    };

    /*
    enum CompareScope {
        cName = 0,      // Compare Full Name Only
        cFull = 1,      // Compare entire record (ID, Google details, the lot)
        cDetails = 2,   // Compare just the local details
        cID = 3,        // Compare the ID Only
        cGoogleID = 4   // Compare the Google ID Only
    };
*/

public:
    QString *_cid ;
    QString emptystring ;

    QString googleXml ;
    QString filedata[LASTRECORD+1] ;
    QString sortstring ;

    QString getOverviewResponse ;
    QString getOverviewResponseHtml ;

    bool isnull ;
    bool isdirty ;
    bool isempty ;

    bool getOverviewDirty ;
    bool getOverviewHtmlDirty ;

    QDateTime googleSaveDateTimeResponse ;
    QDateTime contactSaveDateTimeResponse ;
    QString firstname, surname, organisation ;
    QString formattedNameResponse ;
    History history ;
    Todo todo ;

public:

    Contact();
    ~Contact() ;

    char *getContactRecordName(enum ContactRecord field) ;

    // Used to set nullentries
    bool setNull() ;
    bool isNull() ;

    bool isEmpty() ;
    bool isDirty() ;

    bool isForAccount(QString googleaccount) ;
    bool changedSinceSync(QDateTime &syncdate) ;

    bool createNew() ;
    bool save(QString path, Encryption *enc) ;
    bool load(QString path, QString filename, Encryption *enc) ;
    bool reloadJournal(QString path) ;
    bool reloadTodo(QString path) ;

    void markAsSaved() ;
    void markAsDirty() ;

    int find(QString text) ;

    /*
    bool compare(Contact &other, enum CompareScope scope) ;
    */

    Contact& getThis() ;
    History& getHistory() ;
    Todo& getTodo() ;

    void setField(enum ContactRecord type, QString data) ;
    void setFlag(enum ContactRecord field, bool flag) ;
    void setDate(enum ContactRecord field, QDateTime data) ;

    QString &getField(enum ContactRecord type);
    bool isSet(enum ContactRecord field);
    bool isTristate(enum ContactRecord field);
    QDateTime& getDate(enum ContactRecord field) ;

    QString& getGoogleRecordId() ;
    QString &getFormattedName(bool includeorganisation=true, bool surnamefirst=true) ;
    QString &getOverview(enum ContactOverviewType overviewtype) ;
    QString &asText() ;

    QString &parsePhoneNumber(QString src) ;
    QString &parseDate(QString src) ;

    // Copy selected fields to destination
    Contact& copyGoogleAccountFieldsTo(Contact& dest) ;
    Contact& copySyncedFieldsTo(Contact& dest) ;

    bool matches(Contact &with) ;

    Contact& operator=(const Contact &rhs);

    int operator==(const Contact &rhs) const;
    int operator<(const Contact &rhs) const;


};

#endif // CUSTOMERRECORD_H
