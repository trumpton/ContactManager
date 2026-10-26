//
//  GoogleAccess - parseContact
//
//  Parses supplied json data and populates the given contact structure
//
// contact        - The contact structure to use in the update
//
#include "googleaccess.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

// Trap ID For Testing Breakpoint
#define DEBUGTRAPID "f9acd413-4f83-47e7-a62e-f653d59c6044"

// Namespace for the local IDs made from Google contact resource names (see parseContact)
static const QUuid googleContactIdNamespace("3e8b5a17-6c2d-4f90-a1b4-7d9e2c58f0a3") ;


bool GoogleAccess::parseContact(QString &json, QString &tag, Contact &contact)
{
    QJsonParseError err;
    QJsonDocument doc ;
    doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

    if (err.error != QJsonParseError::NoError) return false ;
    if (doc.isNull()) return false ;
    if (!doc.isObject()) return false ;

    QJsonObject jsonobject = doc.object() ;
    if (tag.isEmpty()) {
        return parseContact(jsonobject, contact);
    } else {
        QJsonObject tagged = jsonobject[tag].toObject() ;
        return parseContact(tagged, contact) ;
    }
}


// Returns true if metadata is of given type


bool GoogleAccess::isMetadataFromContacts(QJsonValue object) {
    return (getMetadataField(object).compare("CONTACT") == 0) ;
}

bool GoogleAccess::isMetadataFromProfile(QJsonValue object) {
    return (getMetadataField(object).compare("PROFILE") == 0) ;
}

bool GoogleAccess::isMetadataPrimary(QJsonValue object) {
    return (getMetadataField(object, true).compare("true") == 0) ;
}

QString GoogleAccess::getMetadataField(QJsonValue object, bool primary)
{
    // "metadata": {
    //   "primary": true,
    //   "source": { "type": "CONTACT" }
    // }
    QJsonValue metadata = object.toObject()["metadata"] ;
    if (!metadata.isObject()) return QString("") ;
    if (primary) {
        QJsonValue primary = metadata.toObject()["primary"].toObject() ;
        if (primary.isBool() && primary.toBool()) return QString("true") ;
        else return QString("false") ;
    } else {
        QJsonObject source = metadata.toObject()["source"].toObject() ;
        if (source.isEmpty()) return QString("") ;
        QJsonValue type = source["type"];
        if (!type.isString()) return QString("") ;
        else return type.toString() ;
    }
}


//
// TODO - ONLU USE METADATA COMMENTS
// STORE METADATA PROFILE IN ADDITIONAL FIELDS FOR INFO
//
bool GoogleAccess::parseContact(QJsonObject &item, Contact &contact)
{
    QString contactmanagerid ;
    bool hasid = false ;

    // Google ETag, Account and Id
    contact.setField(Contact::GoogleRecordId, item["resourceName"].toString()) ;
    contact.setField(Contact::GoogleEtag, item["etag"].toString()) ;

    // Metadata deleted
    QJsonObject metadata = item["metadata"].toObject() ;
    QJsonValue isdeleted = metadata["deleted"] ;
    if (isdeleted.isBool()) contact.setFlag(Contact::Deleted, isdeleted.toBool()) ;

    // Metadata Google Last Updated Field
    QDateTime latestdate ;
    QJsonArray sourcesarray = metadata["sources"].toArray() ;
    foreach (const QJsonValue& sourceitem, sourcesarray) {
        if (sourceitem.isObject()) {
            // Format: 2018-05-02T17:33:00.491001Z
            QString timestr = sourceitem.toObject()["updateTime"].toString() ;
            if (!timestr.isEmpty()) {
                QDate date = date.fromString(timestr.mid(0,10), "yyyy-MM-dd") ;
                QTime time = time.fromString(timestr.mid(11,12), "HH:mm:ss.zzz") ;
                if (date.isValid() && time.isValid()) {
                    if (!latestdate.isValid() || QDateTime(date, time, Qt::UTC)>latestdate) {
                        latestdate = QDateTime(date, time, Qt::UTC) ;
                    }
                }
            }
        }
    }
    if (latestdate.isValid()) {
        contact.setField(Contact::Updated, latestdate.toString()) ;
    }

    // Extended / Private Properties
    QJsonArray extendedproperties = item["userDefined"].toArray() ;
    foreach (const QJsonValue& extendeditem, extendedproperties) {
        if (extendeditem.isObject() && isMetadataFromContacts(extendeditem)) {
            QJsonObject obj = extendeditem.toObject() ;
            QString name = obj["key"].toString() ;
            QString value = obj["value"].toString() ;
            if (name.compare("id")==0 && !value.isEmpty()) {
                contact.setField(Contact::ID,value) ;
                hasid = true ;
            }
            if (name.compare("textme")==0) contact.setField(Contact::TextMe,value) ;
            if (name.compare("emailme")==0) contact.setField(Contact::EmailMe,value) ;
            if (name.compare("hidden")==0) contact.setField(Contact::Hidden,value) ;
            if (name.compare("uploaded")==0) contact.setField(Contact::Uploaded, value) ;
        }
    }

    // A contact the app has not uploaded has no id property, so the ID it was given is a new random one
    // each time it is downloaded. Derive a stable ID from the Google resource name instead, so the same
    // contact keeps the same local ID from one sync to the next
    QString resourcename = item["resourceName"].toString() ;
    if (!hasid && !resourcename.isEmpty()) {
        contact.setField(Contact::ID, QUuid::createUuidV5(googleContactIdNamespace, resourcename.toUtf8()).toString(QUuid::WithoutBraces)) ;
    }

#ifdef DEBUGTRAPID
    if (contactmanagerid.compare(DEBUGTRAPID)==-0) {
        QString id = contactmanagerid ; // Debug Trap
    }
#endif


    // Surname
    // Names
    QJsonArray namesarray = item["names"].toArray() ;
    foreach (const QJsonValue& namesitemvalue, namesarray) {
        if (namesitemvalue.isObject())  {

            QJsonObject nameitem = namesitemvalue.toObject() ;
            // QString name = nameitem["displayName"].toString() ;
            QString familyname = nameitem["familyName"].toString() ;
            QString middlename = nameitem["middleName"].toString() ;
            QString givenname = nameitem["givenName"].toString() ;
            if (!middlename.isEmpty() && givenname.isEmpty()) givenname = middlename ;
            else if (!middlename.isEmpty()) givenname = givenname + QString(" ") + middlename ;

            if (isMetadataFromContacts(namesitemvalue)) {
                if (!familyname.isEmpty()) contact.setField(Contact::Surname, familyname) ;
                if (!givenname.isEmpty()) contact.setField(Contact::Names, givenname) ;
            } else if (isMetadataFromProfile(namesitemvalue)) {
                if (!familyname.isEmpty()) contact.setField(Contact::ProfileSurname, familyname) ;
                if (!givenname.isEmpty()) contact.setField(Contact::ProfileNames, givenname) ;
            }
        }
    }


    // Birthday
    QString birthday ;
    QJsonArray birthdaysarray = item["birthdays"].toArray() ;
    if (birthdaysarray.count()>0) {
        QJsonObject bdayitem = birthdaysarray.at(0).toObject() ;
        QJsonObject dateitem = bdayitem["date"].toObject() ;
        int day = dateitem["day"].toInt() ;
        int month = dateitem["month"].toInt() ;
        if (dateitem["year"].isNull()) {
            birthday = QString("-%1-%2").arg(month, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0')) ;
        } else {
            int year = dateitem["year"].toInt() ;
            birthday = QString("%1-%2-%3").arg(year, 4, 10).arg(month, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0')) ;
        }
        contact.setField(Contact::Birthday,birthday) ;
    }


    // Home
    // Work
    // Mobile
    // Phone2 (&Phone2Title)
    // Phone3 (&Phone3Title)
    // Phone4 (&Phone4Title)
    // Phone: six slots, each with a label.  A number goes to the slot for its Google type when that slot is free
    // (home to Phone, work to Work, mobile to Mobile, other to Phone2).  The rest go to the first free slot.  Each
    // number keeps its Google type as its label.  A number with no slot left is kept in Comments.
    struct PendingPhone { QString number ; QString label ; } ;
    QList<PendingPhone> pendingphones ;
    QJsonArray phonenumbers = item["phoneNumbers"].toArray() ;
    foreach (const QJsonValue& phoneitem, phonenumbers) {

        if (phoneitem.isObject()) {

            QJsonObject phonenumber = phoneitem.toObject() ;

            if (isMetadataFromProfile(phoneitem)) {
                contact.setField(Contact::ProfilePhone, phonenumber["value"].toString().replace(" ","")) ;
            }

            if (isMetadataFromContacts(phoneitem)) {
                QString number = phonenumber["value"].toString().replace(" ","") ;
                if (number.isEmpty()) continue ;
                QString type = phonenumber["type"].toString().trimmed() ;
                if (type.isEmpty()) type = "Other" ;
                QString label = type.left(1).toUpper() + type.mid(1) ;
                pendingphones << PendingPhone{ number, label } ;
            }
        }
    }

    const Contact::ContactRecord phonefields[6] = { Contact::Phone, Contact::Work, Contact::Mobile, Contact::Phone2, Contact::Phone3, Contact::Phone4 } ;
    const Contact::ContactRecord phonetitles[6] = { Contact::PhoneTitle, Contact::WorkTitle, Contact::MobileTitle, Contact::Phone2Title, Contact::Phone3Title, Contact::Phone4Title } ;
    bool phoneused[6] = { false, false, false, false, false, false } ;
    QList<bool> phoneplaced(pendingphones.size(), false) ;
    for (int i=0; i<pendingphones.size(); i++) {
        QString lower = pendingphones.at(i).label.toLower() ;
        int want = (lower=="home") ? 0 : (lower=="work") ? 1 : (lower=="mobile") ? 2 : (lower=="other") ? 3 : -1 ;
        if (want>=0 && !phoneused[want]) {
            contact.setField(phonefields[want], pendingphones.at(i).number) ;
            contact.setField(phonetitles[want], pendingphones.at(i).label) ;
            phoneused[want] = true ;
            phoneplaced[i] = true ;
        }
    }
    QStringList extraphones ;
    for (int i=0; i<pendingphones.size(); i++) {
        if (phoneplaced[i]) continue ;
        int slot = -1 ;
        for (int s=0; s<6 && slot<0; s++) if (!phoneused[s]) slot = s ;
        if (slot<0) {
            extraphones << pendingphones.at(i).number ;
        } else {
            contact.setField(phonefields[slot], pendingphones.at(i).number) ;
            contact.setField(phonetitles[slot], pendingphones.at(i).label) ;
            phoneused[slot] = true ;
        }
    }

    // Find the groups
    // Items in myContacts group are not hidden
    contact.setFlag(Contact::GroupBusiness, false) ;
    contact.setFlag(Contact::GroupClient, false) ;
    contact.setFlag(Contact::GroupFamily, false) ;
    contact.setFlag(Contact::GroupFriend, false) ;
    contact.setFlag(Contact::GroupOther, false) ;
    QJsonArray groups = item["memberships"].toArray() ;
    foreach (const QJsonValue& groupitem, groups) {
        if (groupitem.isObject()) {
            QJsonObject value = groupitem.toObject() ;
            QJsonObject cgm = value["contactGroupMembership"].toObject() ;
            QString thisgroup = cgm["contactGroupId"].toString() ;
            if (thisgroup.compare(gidbusiness)==0) { contact.setFlag(Contact::GroupBusiness, true) ; }
            else if (thisgroup.compare(gidclient)==0) { contact.setFlag(Contact::GroupClient, true) ; }
            else if (thisgroup.compare(gidfamily)==0) { contact.setFlag(Contact::GroupFamily, true) ; }
            else if (thisgroup.compare(gidfriend)==0) { contact.setFlag(Contact::GroupFriend, true) ; }
            else if (thisgroup.compare(gidother)==0) { contact.setFlag(Contact::GroupOther, true) ; }
        }
    }

    // Organisation (Select 'work' in preference to anything else)
    QString workorg, otherorg ;
    QString workorgprof, otherorgprof ;
    QJsonArray organisations = item["organizations"].toArray() ;
    foreach (const QJsonValue& organisationitem, organisations) {
        QString type = organisationitem.toObject()["type"].toString().toLower() ;
        QString value = organisationitem.toObject()["name"].toString() ;

        if (isMetadataFromProfile(organisationitem)) {
            if (type.compare("work")==0) workorgprof=value ;
            else otherorgprof=value ;
        } else if (isMetadataFromContacts(organisationitem)) {
            if (type.compare("work")==0) workorg=value ;
            else otherorg=value ;
        }
    }

    if (workorgprof.isEmpty()) workorgprof=otherorgprof ;
    if (workorg.isEmpty()) workorg=otherorg ;
    if (workorg.isEmpty()) workorg=otherorgprof ;

    contact.setField(Contact::ProfileOrg, workorgprof) ;
    contact.setField(Contact::Organisation, workorg) ;

    // Address1
    // Address2
    // Address3
    QString address1, address2, address3 ;
    QJsonArray addresses = item["addresses"].toArray() ;

    // Find address / other addresses
    for (int i=0; i<addresses.size(); i++) {
        if (isMetadataFromContacts(addresses[i])) {
            QString type = addresses[i].toObject()["type"].toString().toLower() ;
            QString formattedaddress = addresses[i].toObject()["formattedValue"].toString() ;
            // An address with no type is taken as the first address, as it is when it is the only one
            if (type.isEmpty() || type.compare("address")==0 || type.compare("primary")==0 || type.compare("work")==0 || type.compare("home")==0) {
                if (address1.isEmpty()) {
                    address1 = formattedaddress ;
                } else {
                    address3 = formattedaddress ;
                }
            } else if (address2.isEmpty()) address2 = formattedaddress ;
            else address3 = formattedaddress ;
        }
    }

    // Just in case there are two addresses, and the first isn't address/primary/work/home
    // Or there are two addresses, both of type address/primary/work/home
    if (address1.isEmpty()) address1 = address3 ;
    if (address2.isEmpty()) address2 = address3 ;

    // And store
    if (!address1.isEmpty()) contact.setField(Contact::Address, address1) ;
    if (!address2.isEmpty()) contact.setField(Contact::Address2, address2) ;

    // Webaddress (type=url)
    QJsonArray websites = item["urls"].toArray() ;
    foreach (const QJsonValue& urlitem, websites) {
        if (urlitem.isObject() && isMetadataFromContacts(urlitem)) {
            QJsonObject url = urlitem.toObject() ;
            QString type = url["type"].toString() ;
            if (type.compare("homePage")==0) {
                QString value = url["value"].toString();
                contact.setField(Contact::Webaddress,value) ;
            }
        }
    }

    // Email: four slots, each with a label.  An address goes to the slot for its Google type when that slot is
    // free (home to the first, other to the second, work to the third).  The rest go to the first free slot.
    // Each address keeps its Google type as its label.  An address with no slot left is kept in Comments.
    struct PendingEmail { QString address ; QString label ; } ;
    QList<PendingEmail> pending ;
    QJsonArray emailaddresses = item["emailAddresses"].toArray() ;
    foreach (const QJsonValue& emailitem, emailaddresses) {

        if (!emailitem.isObject()) continue ;

        QJsonObject emailaddress = emailitem.toObject() ;
        QString address = emailaddress["value"].toString().trimmed() ;

        if (isMetadataFromProfile(emailitem)) {
            if (contact.getField(Contact::ProfileEMail).isEmpty())
                contact.setField(Contact::ProfileEMail, address) ;

        } else if (isMetadataFromContacts(emailitem) && !address.isEmpty()) {

            // A custom label comes back as type "custom", with the label as its formatted type
            QString type = emailaddress["type"].toString().trimmed() ;
            if (type.isEmpty() || type.compare("custom", Qt::CaseInsensitive)==0) type = emailaddress["formattedType"].toString().trimmed() ;
            if (type.isEmpty()) type = "Other" ;
            QString label = type.left(1).toUpper() + type.mid(1) ;
            pending << PendingEmail{ address, label } ;
        }
    }

    const Contact::ContactRecord emailfields[4] = { Contact::Email, Contact::Email2, Contact::Email3, Contact::Email4 } ;
    const Contact::ContactRecord titlefields[4] = { Contact::EmailTitle, Contact::Email2Title, Contact::Email3Title, Contact::Email4Title } ;
    bool used[4] = { false, false, false, false } ;
    QList<bool> placed(pending.size(), false) ;

    // The slot for each Google type, when it is free
    for (int i=0; i<pending.size(); i++) {
        QString lower = pending.at(i).label.toLower() ;
        int want = (lower=="home") ? 0 : (lower=="other") ? 1 : (lower=="work") ? 2 : -1 ;
        if (want>=0 && !used[want]) {
            contact.setField(emailfields[want], pending.at(i).address) ;
            contact.setField(titlefields[want], pending.at(i).label) ;
            used[want] = true ;
            placed[i] = true ;
        }
    }

    // Everything else takes the first free slot
    QStringList extraemails ;
    for (int i=0; i<pending.size(); i++) {
        if (placed[i]) continue ;
        int slot = -1 ;
        for (int s=0; s<4 && slot<0; s++) if (!used[s]) slot = s ;
        if (slot<0) {
            extraemails << pending.at(i).address ;
        } else {
            contact.setField(emailfields[slot], pending.at(i).address) ;
            contact.setField(titlefields[slot], pending.at(i).label) ;
            used[slot] = true ;
        }
    }

    // SIP Addresses
    // store whatever the type
    QJsonArray sipaddresses = item["sipAddresses"].toArray() ;
    foreach (const QJsonValue& sipitem, sipaddresses) {

        if (sipitem.isObject()) {

            QJsonObject sipaddress = sipitem.toObject() ;
            //QString type = sipaddress["type"].toString().toLower() ;
            QString address = sipaddress["value"].toString() ;

            if (isMetadataFromContacts(sipitem)) {
                contact.setField(Contact::Voip, address) ;
            }
        }
    }

    // Comments
    QJsonArray notes = item["biographies"].toArray() ;
    if (notes.count()>0) {
        if (isMetadataFromContacts(notes[0])) {
            contact.setField(Contact::Comments, notes[0].toObject()["value"].toString()) ;
        }
    }

    // Only four email addresses and six phone numbers are held.  Any others are kept in Comments, so that nothing is lost.
    foreach (const QString &extra, extraemails + extraphones) {
        QString note = QString("Also on Google: %1").arg(extra) ;
        QString comments = contact.getField(Contact::Comments) ;
        if (!comments.contains(note)) contact.setField(Contact::Comments, comments.isEmpty() ? note : comments + "\n" + note) ;
    }


    // Set defaults
    contact.markAsSaved() ;

    bool ok ;
    ok = !contact.getField(Contact::GoogleRecordId).isEmpty() ;
    ok &= !contact.getField(Contact::GoogleEtag).isEmpty() ;

    return ok ;
}
