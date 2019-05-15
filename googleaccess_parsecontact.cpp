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

// Trap ID For Testing Breakpoint
#define DEBUGTRAPID "f9acd413-4f83-47e7-a62e-f653d59c6044"


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
            if (name.compare("id")==0) contact.setField(Contact::ID,value) ;
            if (name.compare("textme")==0) contact.setField(Contact::TextMe,value) ;
            if (name.compare("emailme")==0) contact.setField(Contact::EmailMe,value) ;
            if (name.compare("hidden")==0) contact.setField(Contact::Hidden,value) ;
        }
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
    QJsonArray phonenumbers = item["phoneNumbers"].toArray() ;
    foreach (const QJsonValue& phoneitem, phonenumbers) {

        if (phoneitem.isObject()) {

            QJsonObject phonenumber = phoneitem.toObject() ;

            if (isMetadataFromProfile(phoneitem)) {
                contact.setField(Contact::ProfilePhone, phonenumber["value"].toString().replace(" ","")) ;
            }

            if (isMetadataFromContacts(phoneitem)) {

                // Calculate next free slot

                int nextfree = 0 ;
                if (!contact.getField(Contact::Phone2).isEmpty()) {
                    nextfree++ ;
                    if (!contact.getField(Contact::Phone3).isEmpty()) {
                        nextfree++ ;
                        if (!contact.getField(Contact::Phone4).isEmpty()) {
                            // No more free number slots ignore everything else
                            nextfree=-1 ;
                        }
                    }
                }

                if (phonenumber["type"].toString().compare("home")==0) {
                    contact.setField(Contact::Phone,phonenumber["value"].toString().replace(" ","")) ;
                } else if (phonenumber["type"].toString().compare("work")==0) {
                    contact.setField(Contact::Work,phonenumber["value"].toString().replace(" ","")) ;
                } else if (phonenumber["type"].toString().compare("mobile")==0) {
                    contact.setField(Contact::Mobile,phonenumber["value"].toString().replace(" ","")) ;
                } else if (nextfree>=0) {
                    Contact::ContactRecord sTitle = (Contact::ContactRecord) ((int)Contact::Phone2Title + nextfree) ;
                    Contact::ContactRecord sNumber = (Contact::ContactRecord) ((int)Contact::Phone2 + nextfree) ;
                    QString number = phonenumber["value"].toString().replace(" ","") ;
                    QString label = phonenumber["type"].toString() ;
                    if (label.isEmpty()) label = "other " + QString::number(nextfree+1) ;
                    contact.setField(sTitle, label) ;
                    contact.setField(sNumber, number) ;
                }
            }
        }
    }
    contact.sortPhoneNumbers() ;

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
            if (type.compare("address")==0 || type.compare("primary")==0 || type.compare("work")==0 || type.compare("home")==0) {
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

    // Email
    // Email2
    QString email1, email2, email3 ;
    QJsonArray emailaddresses = item["emailAddresses"].toArray() ;
    foreach (const QJsonValue& emailitem, emailaddresses) {

        if (emailitem.isObject()) {

            QJsonObject emailaddress = emailitem.toObject() ;
            QString type = emailaddress["type"].toString().toLower() ;
            QString address = emailaddress["value"].toString() ;

            if (isMetadataFromProfile(emailitem)) {
                if (contact.getField(Contact::ProfileEMail).isEmpty())
                    contact.setField(Contact::ProfileEMail, address) ;

            } else if (isMetadataFromContacts(emailitem)) {
                if (type.compare("email")==0 || type.compare("primary")==0 || type.compare("work")==0 || type.compare("home")==0) {
                    if (email1.isEmpty()) {
                        email1 = address ;
                    } else {
                        email3 = address ;
                    }
                } else if (email2.isEmpty()) email2 = address ;
                else email3 = address ;
            }
        }
    }

    // Just in case there are two addresses, and the first isn't email/primary/work/home
    // Or there are two addresses, both of type email/primary/work/home
    if (email1.isEmpty()) email1 = email3 ;
    if (email2.isEmpty()) email2 = email3 ;

    // And Store
    if (!email1.isEmpty()) contact.setField(Contact::Email,email1) ;
    if (!email2.isEmpty()) contact.setField(Contact::Email2,email2) ;


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


    // Set defaults
    contact.markAsSaved() ;

    bool ok ;
    ok = !contact.getField(Contact::GoogleRecordId).isEmpty() ;
    ok &= !contact.getField(Contact::GoogleEtag).isEmpty() ;

    return ok ;
}
