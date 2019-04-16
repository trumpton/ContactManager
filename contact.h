#ifndef CUSTOMERRECORD_H
#define CUSTOMERRECORD_H

// TODO: Check to see if we really need cached copies of strings

#include <QString>
#include <QDateTime>
#include "history.h"
#include "todo.h"
#include "../Lib/encryption.h"


class Contact
{

public:

    enum ContactOverviewType {
        contactAsText = 0,
        contactAsHTML = 1,
        contactAsVCARD = 2
    };

    enum ContactRecord {
        Surname=0, Names, Organisation, Address, Address2,
        Phone, Work, Mobile, Phone2, Phone3, Phone4, Voip,
        Phone2Title, Phone3Title, Phone4Title,
        Email, Email2, Webaddress, Birthday, Comments,
        GroupBusiness, GroupClient, GroupFamily, GroupFriend, GroupOther,
        Hidden, EmailMe, TextMe, Deleted,
        ID, Updated, Created,
        ProfileSurname, ProfileNames, ProfileAddress, ProfilePhone, ProfileEMail, ProfileOrg,
        GoogleRecordId,
        GoogleEtag,
        ToBeUploaded, ToBeDownloaded,
        NumberOfRecords
    };

private:
    struct ContactRecordInfo {
        enum ContactRecord recordtype;
        bool isflag ;            // True if entry is a boolean
        bool issynced ;          // True if entry is synced (i.e. changes require uploading)
        bool updatesdirty ;      // True if entry affects dirty flag (and Updated)
        bool isprofile ;         // True if info is from google profile
        bool isdetails ;         // True if entry contains field details
        bool isgroupdetails ;    // True if entry contains group field details
        bool issaved ;           // True if entry is saved / loaded
        bool NOTUSED2 ;          //
        char name[16] ;
    } ;

//    ContactRecordInfo const contactrecordinfo[NumberOfRecords][sizeof(ContactRecordInfo)] = {
    ContactRecordInfo const contactrecordinfo[NumberOfRecords] = {
        //                  flag   sync  dirty  profl  detls  group  saved  unused
        { Surname,         false,  true,  true, false,  true, false,  true, false, "surname" },       // Surname
        { Names,           false,  true,  true, false,  true, false,  true, false, "names" },         // Firstname MiddleName
        { Organisation,    false,  true,  true, false,  true, false,  true, false, "organisation"},   // Organisation
        { Address,         false,  true,  true, false,  true, false,  true, false, "address"},        // First Address
        { Address2,        false,  true,  true, false,  true, false,  true, false, "address2"},       // Second Address
        { Phone,           false,  true,  true, false,  true, false,  true, false, "phone"},          // Home Phone Number
        { Work,            false,  true,  true, false,  true, false,  true, false, "work"},           // Work / Business Phone Number
        { Mobile,          false,  true,  true, false,  true, false,  true, false, "mobile"},         // Mobile Phone Number
        { Phone2,          false,  true,  true, false,  true, false,  true, false, "phone2"},         // Extra Phone Number
        { Phone3,          false,  true,  true, false,  true, false,  true, false, "phone3"},         // Extra Phone Number
        { Phone4,          false,  true,  true, false,  true, false,  true, false, "phone4"},         // Extra Phone Number
        { Voip,            false, false,  true, false,  true, false,  true, false, "voip"},           // VOIP Number (Not Currently Used)
        { Phone2Title,     false,  true,  true, false,  true, false,  true, false, "phone2title"},    // Title of phone2
        { Phone3Title,     false,  true,  true, false,  true, false,  true, false, "phone3title"},    // Title of phone3
        { Phone4Title,     false,  true,  true, false,  true, false,  true, false, "phone4title"},    // Title of phone4
        { Email,           false,  true,  true, false,  true, false,  true, false, "email" },         // Primary email address
        { Email2,          false,  true,  true, false,  true, false,  true, false, "email2"},         // Alt email address
        { Webaddress,      false,  true,  true, false,  true, false,  true, false, "webaddress"},     // Website
        { Birthday,        false,  true,  true, false,  true, false,  true, false, "birthday"},       // Date of Birth
        { Comments,        false,  true,  true, false,  true, false,  true, false, "comments"},       // Comments
        { GroupBusiness,    true,  true,  true, false, false,  true,  true, false, "groupbusiness"},  // True if user in business group
        { GroupClient,      true,  true,  true, false, false,  true,  true, false, "groupclient"},    // True if user in client group
        { GroupFamily,      true,  true,  true, false, false,  true,  true, false, "groupfamily"},    // True if user in family group
        { GroupFriend,      true,  true,  true, false, false,  true,  true, false, "groupfriend"},    // True if user in friend group
        { GroupOther,       true,  true,  true, false, false,  true,  true, false, "groupother"},     // True if user in other group
        { Hidden,           true,  true,  true, false,  true, false,  true, false, "hidden"},         // True if Hidden flag is set
        { EmailMe,          true,  true,  true, false,  true, false,  true, false, "emailme"},        // True if EmailMe flag set
        { TextMe,           true,  true,  true, false,  true, false,  true, false, "textme"},         // True if TextMe flag set
        { Deleted,          true,  true,  true, false,  true, false,  true, false, "deleted"},        // True if entry has been deleted
        { ID,              false,  true,  true, false, false, false,  true, false, "id"},             // Contact Manager ID
        { Updated,         false, false, false, false, false, false,  true, false, "updated"},        // Time Date entry was updated
        { Created,         false, false, false, false, false, false,  true, false, "created"},        // Time Date entry was created
        { ProfileSurname,  false, false,  true,  true, false, false,  true, false, "profilesurname"}, // Surname from profile
        { ProfileNames,    false, false,  true,  true, false, false,  true, false, "profilenames"},   // Names from profile
        { ProfileAddress,  false, false,  true,  true, false, false,  true, false, "profileaddress"}, // Address from profile
        { ProfilePhone,    false, false,  true,  true, false, false,  true, false, "profilephone"},   // Phone number from profile
        { ProfileEMail,    false, false,  true,  true, false, false,  true, false, "profileemail"},   // Email from profile
        { ProfileOrg,      false, false,  true,  true, false, false,  true, false, "profileorg"},     // Organisation from profile
        { GoogleRecordId,  false, false,  true, false, false, false,  true, false, "googlerecordid"}, // Google record ID
        { GoogleEtag,      false, false,  true, false, false, false, false, false, "googleetag"},     // Google Etag from last update
        { ToBeUploaded,     true, false, false, false, false, false, false, false, "tobeuploaded"},   // Flag to track if google upload required
        { ToBeDownloaded,   true, false, false, false, false, false, false, false, "tobedownloaded"}, // Flag to track if google download required
    };

private:
    bool contactrecordinfook ;
    QString *_cid ;
    QString emptystring ;

    QString googleXml ;
    QString filedata[NumberOfRecords] ;
    QStringList mergedidlist ;
    QString sortstring ;

    QString getOverviewResponse ;
    QString getOverviewResponseHtml ;

    bool isnew ;
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

    Contact(bool isNull=false);
    ~Contact() ;

    // Test contact record field types
//    bool isRecordFlag(enum ContactRecord field) ;
    char *contactRecordName(enum ContactRecord field) ;
//    bool isRecordSynced(enum ContactRecord field) ;

    // Used to set nullentries
    bool setNull() ;
    bool isNull() ;

    bool isEmpty() ;
    bool isDirty() ;

    // Set following new, until load or first save
    bool isNew() ;

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

    // Merge
    void mergeInto(Contact& other) ;

    // Field and Flag Access
    void setField(enum ContactRecord type, QString data) ;
    void setFlag(enum ContactRecord field, bool flag) ;
    void setDate(enum ContactRecord field, QDateTime data) ;

    QString &getField(enum ContactRecord type);
    bool isSet(enum ContactRecord field);
    bool isTristate(enum ContactRecord field);
    QDateTime& getDate(enum ContactRecord field) ;

    bool sortPhoneNumbers() ;

    QString& getGoogleRecordId() ;
    QString &getFormattedName(bool includeorganisation=true, bool surnamefirst=true) ;
    QString &getOverview(enum ContactOverviewType overviewtype) ;
    QString &asText() ;

    QString &parsePhoneNumber(QString src) ;
    QString &parseDate(QString src) ;

    // Merged ID
    int mergedIdCount() ;
    void appendMergedId(QString id) ;
    QStringList& mergedIdList() ;

    // Match and Copy Masks (mctype)
    static const int mcId=0x01 ;
    static const int mcGoogleId=0x02 ;
    static const int mcDetails=0x04 ;
    static const int mcGroup=0x08 ;
    static const int mcProfile=0x10 ;
    static const int mcFlag=0x20 ;
    static const int mcEtag=0x80 ;
    static const int mcControlFlags=0x100 ;

    // Match and Copy Masks (Etag is specifically excluded)
    static const int mcDetailsGroupProfile = 0x1C ;
    static const int mcDetailsGroup = 0x0C ;

    // Returns true if the contactrecord 'i' is of type mctype
    bool isContactOfType(ContactRecord i, int mctype) ;

    // Test current contact against reference
    bool matches(Contact &with, int mctype) ;
    QString mismatch(Contact &with, int mctype, bool showboth=false) ;


    // Copy selected fields to new contact
    bool copyTo(Contact &other, int mctype) ;

    // Copy all data, fields, records (except for IDs and ETag)
    Contact& operator=(const Contact &rhs);

    // Operators used for sorting
    int operator==(const Contact &rhs) const;
    int operator<(const Contact &rhs) const;

};

#endif // CUSTOMERRECORD_H
