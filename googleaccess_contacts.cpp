
#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "configuration.h"
#include <QUrl>

#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>

// Schemas
#define sHOME (char *)"http://schemas.google.com/g/2005#home"
#define sWORK (char *)"http://schemas.google.com/g/2005#work"
#define sMOBILE (char *)"http://schemas.google.com/g/2005#mobile"
#define sOTHER (char *)"http://schemas.google.com/g/2005#other"
#define sHOMEPAGE (char *)"home-page"

//
//  GOOGLE CONTACTS
//
//   A contact can be in several groups, but this tool assumes that it will be in only one of:
//     Family, Business, Client, Other
//   and
//     MyGroup
//
//  Contact status is defined as follows
//
//             in MyGroup      gd$deleted      Response
//    active      yes          not present     200 OK
//    hidden      no           not present     200 OK
//    deleted     -            present         200 OK
//    deleted     -            -               404 Not Found


bool GoogleAccess::getContacts(ContactDatabase &googlecontacts, int pass)
{
    QJsonParseError err;
    QJsonDocument doc ;
    QString json ;

    // Get the group names
    loadGroups() ;

    googlecontacts.clear() ;

    // TODO: get account
    QString account = gConf->getGoogleUsername() ;

    if (refreshtoken.isEmpty()) return false ;

    QString url = "https://www.google.com/m8/feeds/contacts/" + account.toHtmlEscaped() + "/full" ;
    url = url + "?v=3.0&alt=json" ;
    if (!contactsynctoken.isEmpty()) url += "&updated-min=" + contactsynctoken ;
    url = url + "&showdeleted=true" ;
    url = url + "&sortorder=ascending" ;
    url = url + "&max-results=9999" ;

    // HACK HACK HACK
    // readFromFile(gConf->getDatabasePath() + QString("/InterestingLogs/contactlist-1.json"), json) ;

    // If this returns 403, check that the API is enabled on google
    json = googleGet(url, QString("/contactlist-") + QString::number(pass) + QString(".txt")) ;

    if (!errorstatus.isEmpty()) return false ;

    doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

    if (err.error != QJsonParseError::NoError) return false ;
    if (doc.isNull()) return false ;
    if (!doc.isObject()) return false ;

    QJsonObject obj = doc.object() ;

    // Get the time, to use as the sync token next time around
    QString synctoken = obj["feed"].toObject()["updated"].toObject()["$t"].toString() ;
    if (synctoken.isEmpty()) return false ;
    contactsynctoken = synctoken ;

    // Transfer the contacts to the ContactDatabase
    QJsonArray items = obj["feed"].toObject()["entry"].toArray() ;
    foreach (const QJsonValue & value, items) {

        if (value.isObject()){
            QJsonObject item = value.toObject() ;
            Contact contact ;

            // Add the changes to the contacts list if the parse was
            // successful, even if the contact was deleted
            if (parseContact(item, contact)) {
                Contact& newcontact = googlecontacts.addContact() ;
                newcontact=contact ;
            }
        }
    }
    return true ;
}

bool GoogleAccess::getContact(Contact& contact)
{
    QJsonParseError err;
    QJsonDocument doc ;
    QString json ;
    QString& account = contact.getField(Contact::GoogleAccount) ;
    QString& record = contact.getField(Contact::GoogleRecordId) ;

    // GET https://www.google.com/m8/feeds/contacts/example.com/full/12345
    QString url = "https://www.google.com/m8/feeds/contacts/" +
            account + "/full/" +
            record + "?v=3.0&alt=json" ;

    json = googleGet(url, "contactdetails.txt") ;
    doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

    if (err.error != QJsonParseError::NoError) return false ;
    if (doc.isNull()) return false ;
    if (!doc.isObject()) return false ;

    QJsonObject obj = doc.object()["entry"].toObject() ;
    Contact newcontact ;

    // TODO: This seems to return false for some reason.
    // change local contact and re-sync to get here ...
    if (parseContact(obj, newcontact)) {
         contact = newcontact ;
         return true ;
    } else {
         return false ;
    }
}


bool GoogleAccess::parseContact(QJsonObject &item, Contact &contact)
{
    QString textme ;
    QString emailme ;
    QString hasbeenuploaded ;
    QString contactmanagerid ;
    QString group="" ;

    // Set defaults
    contact.setFlag(Contact::Hidden, true) ;
    contact.markAsSaved() ;

    // Extended / Private Properties
    QJsonArray extendedproperties = item["gd$extendedProperty"].toArray() ;
    foreach (const QJsonValue& item, extendedproperties) {
        if (item.isObject()) {
            QJsonObject obj = item.toObject() ;
            QString name = obj["name"].toString() ;
            QString value = obj["value"].toString() ;
            if (name.compare("contactmanagerid")==0) contactmanagerid = value ;
            if (name.compare("textme")==0) textme = value ;
            if (name.compare("emailme")==0) emailme = value ;
            if (name.compare("hasbeenuploaded")==0) hasbeenuploaded = value ;
        }
    }

    if (contactmanagerid.compare("f9acd413-4f83-47e7-a62e-f653d59c6044")==-0) {
        QString id = contactmanagerid ; // Debug Trap
    }

    // Set the flags
    if (!textme.isEmpty()) contact.setField(Contact::TextMe, textme) ;
    if (!emailme.isEmpty()) contact.setField(Contact::EmailMe, emailme) ;
    if (!hasbeenuploaded.isEmpty()) contact.setField(Contact::GoogleUploaded, hasbeenuploaded) ;

    // Set the Contact Manager ID
   if (!contactmanagerid.isEmpty()) {
        contact.setField(Contact::ID, contactmanagerid) ;
    }

    // Find the groups
    QJsonArray groups = item["gContact$groupMembershipInfo"].toArray() ;

    // Items in MyContact group are not hidden
    bool hidden = true ;

    foreach (const QJsonValue& item, groups) {
        if (item.isObject()) {
            QJsonObject value = item.toObject() ;
            QString href = value["href"].toString() ;
            QString deleted = value["deleted"].toString() ;
            if (deleted.compare("true")!=0) {
                if (href.compare(findMyContactsId())==0) hidden=false ;
                else group = findGroupName(href) ;
            }
        }
    }
    if (!group.isEmpty()) contact.setField(Contact::Group, group) ;
    if (!hidden) contact.setFlag(Contact::Hidden, false) ;

    // Surname
    // Names
    QString surname = "" ;
    QString names = "" ;
    QJsonObject name = item["gd$name"].toObject() ;
    QJsonObject familyname = name["gd$familyName"].toObject() ;
    QJsonObject givenname = name["gd$givenName"].toObject() ;
    if (!name.isEmpty() && !familyname.isEmpty())
        surname = familyname["$t"].toString() ;
    if (!name.isEmpty() && !givenname.isEmpty())
        names = givenname["$t"].toString() ;
    if (!surname.isEmpty()) contact.setField(Contact::Surname, surname) ;
    if (!names.isEmpty()) contact.setField(Contact::Names, names) ;



    // Birthday
    QString birthday = item["gContact$birthday"].toObject()["when"].toString() ;
    if (!birthday.isEmpty()) contact.setField(Contact::Birthday,birthday) ;



    // Comments
    QString comments = item["content"].toObject()["$t"].toString() ;
    if (!comments.isEmpty()) contact.setField(Contact::Comments, comments) ;



    // Organisation
    QJsonArray organisations = item["gd$organization"].toArray() ;
    if (organisations.size()>0) {
        contact.setField(Contact::Organisation, organisations[0].toObject()["gd$orgName"].toObject()["$t"].toString()) ;
    }



    // Address1
    // Address2
    QString address1, address2 ;
    QString home, work, other ;

    QJsonArray addresses = item["gd$structuredPostalAddress"].toArray() ;

    // Find work / home / other addresses
    for (int i=0; i<addresses.size(); i++) {
        QString schema = addresses[i].toObject()["rel"].toString() ;
        QString addr = addresses[i].toObject()["gd$formattedAddress"].toObject()["$t"].toString() ;
        if (schema.compare(sWORK)==0) work = addr ;
        else if (schema.compare(sHOME)==0) home = addr ;
        else other = addr ;
    }

    // Put in address1/address2 depending on whether a business or not
    if (group.compare(gBUSINESS)==0) {
        // work  other
        if (!work.isEmpty() && !other.isEmpty()) { address1 = work ; address2 = other ; }
        //  work  [home]
        else if (!work.isEmpty()) { address1 = work ; address2 = home ; }
        // other
        else if (home.isEmpty()) { address2 = other ; }
        //  [other] home
        else { address1 = other ; address2 = home ; }
    } else {
        // home other
        if (!home.isEmpty() && !other.isEmpty()) { address1 = home ; address2 = other ; }
        //  home [work]
        else if (!home.isEmpty()) { address1 = home ; address2 = work ; }
        // other
        else if (work.isEmpty()) { address2 = other ; }
        //  [other] work
        else { address1 = other ; address2 = work ; }
    }

    // And store
    if (!address1.isEmpty()) contact.setField(Contact::Address, address1) ;
    if (!address2.isEmpty()) contact.setField(Contact::Address2, address2) ;

    // Webaddress
    QJsonArray websites = item["gContact$website"].toArray() ;
    if (websites.size()>0) contact.setField(Contact::Webaddress,websites[0].toObject()["href"].toString()) ;



    // Phone
    // Work
    // Mobile
    // Phone2 (&Phone2Title)
    // Phone3 (&Phone3Title)
    // Phone4 (&Phone4Title)
    QJsonArray phonenumbers = item["gd$phoneNumber"].toArray() ;


    foreach (const QJsonValue& item, phonenumbers) {

        if (item.isObject()) {

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

            QJsonObject phonenumber = item.toObject() ;

            if (phonenumber["rel"].toString().compare(sHOME)==0) {

                contact.setField(Contact::Phone,phonenumber["$t"].toString()) ;

            } else if (phonenumber["rel"].toString().compare(sWORK)==0) {

                contact.setField(Contact::Work,phonenumber["$t"].toString()) ;

            } else if (phonenumber["rel"].toString().compare(sMOBILE)==0) {

                contact.setField(Contact::Mobile,phonenumber["$t"].toString()) ;

            } else if (phonenumber["rel"].toString().compare(sOTHER)==0 && nextfree>=0) {

                Contact::ContactRecord sTitle = (Contact::ContactRecord) ((int)Contact::Phone2Title + nextfree) ;
                Contact::ContactRecord sNumber = (Contact::ContactRecord) ((int)Contact::Phone2 + nextfree) ;
                QString number = phonenumber["$t"].toString() ;
                contact.setField(sTitle, "Other") ;
                contact.setField(sNumber, number) ;

            } else if (nextfree>=0) {

                Contact::ContactRecord sTitle = (Contact::ContactRecord) ((int)Contact::Phone2Title + nextfree) ;
                Contact::ContactRecord sNumber = (Contact::ContactRecord) ((int)Contact::Phone2 + nextfree) ;
                QString number = phonenumber["$t"].toString() ;
                QString label = phonenumber["label"].toString() ;
                if (label.isEmpty()) label = "Other" ;
                contact.setField(sTitle, label) ;
                contact.setField(sNumber, number) ;

            }
        }
    }

    // Email
    // Email2
    QJsonArray emailaddresses = item["gd$email"].toArray() ;
    foreach (const QJsonValue& item, emailaddresses) {
        if (item.isObject()) {
            QJsonObject emailaddress = item.toObject() ;
            if ((group.compare(gBUSINESS)==0 && emailaddress["rel"].toString().compare(sWORK)==0) ||
                (!group.compare(gBUSINESS)==0 && emailaddress["rel"].toString().compare(sHOME)==0)) {
                contact.setField(Contact::Email,emailaddress["address"].toString()) ;
            } else {
                contact.setField(Contact::Email2,emailaddress["address"].toString()) ;
            }
        }
    }

    if (item["gd$deleted"].isObject()) {
        contact.setFlag(Contact::Deleted, true) ;
        contact.setFlag(Contact::GoogleDeleted, true) ;
    } else {
        contact.setFlag(Contact::Deleted, false) ;
        contact.setFlag(Contact::GoogleDeleted, false) ;
    }

    // ETag
    contact.setField(Contact::GoogleEtag, item["gd$etag"].toString()) ;

    // Google Account and Id
    QString googleid = item["id"].toObject()["$t"].toString() ;
    QRegExp rx ;
    rx.setPattern(".*contacts/(.*)/base/(.*)$") ;
    rx.setMinimal(true) ;
    if (rx.indexIn(googleid)>=0) {
        contact.setField(Contact::GoogleAccount, rx.cap(1)) ;
        contact.setField(Contact::GoogleRecordId, rx.cap(2)) ;
    }

    bool ok = !contact.getField(Contact::GoogleAccount).isEmpty() ;
    ok &= !contact.getField(Contact::GoogleRecordId).isEmpty() ;
    ok &= !contact.getField(Contact::GoogleEtag).isEmpty() ;

    return ok ;
}

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



//=====================================================================================================
//
// Public: GoogleAccess - updateContact
//
//  Creates or updates the contact associated with the contact details supplied.
//
// contact        - The contact structure to use in the update
//


bool GoogleAccess::updateContact(Contact &contact, googleAction action)
{
    QString url ;
    bool success=false ;

    // Debug Trap
    if (contact.getField(Contact::ID).compare("14beeb13-f262-4942-88c4-566195772650")==0) {
        int trap = 1 ; trap++ ;
    }

    // Get account
    QString googleaccount = gConf->getGoogleUsername() ;
    // TODO: QUrl::toPercentEncoding
    googleaccount = googleaccount.replace("@", "%40") ;

    // Request to Update a Deleted entry is actually invalid
    // TODO: ERROR TRAP RATHER THAN OVERRIDE
    if (contact.isSet(Contact::Deleted))
        action=GoogleAccess::Delete ;

    // Calculate URL
    if (action==GoogleAccess::Delete) {
        QString googlerecordid = contact.getField(Contact::GoogleRecordId) ;
        // DELETE https://www.google.com/m8/feeds/contacts/example.com/full/12345
        url = "https://www.google.com/m8/feeds/contacts/" + googleaccount + "/full/" + googlerecordid + "?v=3.0&alt=json" ;
    } else if (action==GoogleAccess::Post) {
        url = "https://www.google.com/m8/feeds/contacts/" + googleaccount + "/full?v=3.0&alt=json";
    } else if (action==GoogleAccess::Put) {
        QString googlerecordid = contact.getField(Contact::GoogleRecordId) ;
        url = "https://www.google.com/m8/feeds/contacts/" + googleaccount + "/full/" + googlerecordid + "?v=3.0&alt=json";
    } else {
        // Error
    }

    // Process Request
    if (action==GoogleAccess::Delete) {

/*        // ETag
        QJsonObject entry ;
        QString etag = contact.getField(Contact::GoogleEtag) ;
        entry.insert("gd$etag", etag) ;

        // Encoding & Entry
        QJsonDocument doc ;
        QJsonObject root ;
        root.insert("encoding", gConf->getCodec()) ;
        root.insert("entry", entry) ;
        doc.setObject(root) ;
        QString jsontext = doc.toJson() ;
*/

        googlePutPostDelete(url, action, QString(""), QString("contactupdate.txt")) ;

        if (getNetworkError().isEmpty()) {
            // Deletion success
            success=true ;
            contact.setFlag(Contact::Deleted, true) ;
            contact.setFlag(Contact::GoogleDeleted, true) ;
        } else {
            if (getNetworkErrorCode()==404) {
                // Record has already been purged (does not exist)
                success=true ;
                contact.setFlag(Contact::Deleted, true) ;
                contact.setFlag(Contact::GoogleDeleted, true) ;
            } else {
                success=false ;
            }
        }


     } else {
        // Put / Post

        QJsonObject entry ;

        // ETag
        entry.insert("gd$etag", contact.getField(Contact::GoogleEtag)) ;

        if (!contact.isSet(Contact::Deleted)) {

            // Contact ID & Hidden
            QJsonArray properties ;

            QJsonObject hbuproperty ;
            hbuproperty.insert("name", "hasbeenuploaded") ;
            hbuproperty.insert("value", "true") ;
            properties.append(hbuproperty) ;

            QString textme = contact.getField((Contact::TextMe)) ;
            QString emailme = contact.getField((Contact::EmailMe));

            QJsonObject textmeproperty ;
            textmeproperty.insert("name", "textme") ;
            textmeproperty.insert("value", textme) ;
            properties.append(textmeproperty) ;

            QJsonObject emailmeproperty ;
            emailmeproperty.insert("name", "emailme") ;
            emailmeproperty.insert("value", emailme) ;
            properties.append(emailmeproperty) ;

            QJsonObject idproperty ;
            idproperty.insert("name", "contactmanagerid") ;
            idproperty.insert("value", contact.getField(Contact::ID)) ;
            properties.append(idproperty) ;

            QJsonObject mergeidproperty ;
            mergeidproperty.insert("name", "mergeid-" + contact.getField(Contact::ID)) ;
            mergeidproperty.insert("value", contact.getField(Contact::ID)) ;
            properties.append(mergeidproperty) ;

            entry.insert("gd$extendedProperty", properties) ;

            QJsonArray groups ;
            QJsonObject group ;

            if (!contact.isSet(Contact::Hidden)) {
                group.insert("deleted", "false") ;
                group.insert("href", findMyContactsId()) ;
                groups.append(group) ;

            }

            if (!contact.isSet(Contact::Deleted)) {
                QString groupname = contact.getField(Contact::Group) ;
                QString groupstr = findGroupId(groupname) ;
                if (!groupstr.isEmpty()) {
                    QJsonObject group ;
                    group.insert("deleted", "false") ;
                    group.insert("href", groupstr) ;
                    groups.append(group) ;
                } else {
                    int trap = 1 ; trap++ ;
                }
            }

            if (groups.size()>0) entry.insert("gContact$groupMembershipInfo", groups) ;


            // Surname
            // Names
            QJsonObject gdname ;
            QJsonObject gdgivenname ;
            QJsonObject gdfamilyname ;
            gdgivenname.insert("$t", contact.getField(Contact::Names)) ;
            gdfamilyname.insert("$t", contact.getField(Contact::Surname)) ;
            gdname.insert("gd$givenName", gdgivenname) ;
            gdname.insert("gd$familyName", gdfamilyname) ;
            entry.insert("gd$name", gdname) ;

            // Organisation
            QString& organisationstr = contact.getField(Contact::Organisation) ;
            if (!organisationstr.isEmpty()) {
                QJsonArray gdorganisations ;
                QJsonObject organisation ;
                QJsonObject gdorgname ;
                gdorgname.insert("$t", organisationstr) ;
                organisation.insert("gd$orgName", gdorgname) ;
                organisation.insert("rel", sWORK) ;
                gdorganisations.append(organisation) ;
                entry.insert("gd$organization", gdorganisations) ;
            }

            // Address
            // Address2
            QJsonArray addresses ;

            for (int i=Contact::Address; i<=Contact::Address2; i++) {
                QString& address = contact.getField((Contact::ContactRecord)i) ;
                if (!address.isEmpty()) {
                    QJsonObject item ;
                    QJsonObject gdformattedaddress ;
                    gdformattedaddress.insert("$t", address) ;
                    item.insert("gd$formattedAddress", gdformattedaddress) ;
                    switch (i) {
                    case Contact::Address:
                        if (contact.getField(Contact::Group).compare(gBUSINESS)==0) item.insert("rel", sWORK) ;
                        else item.insert("rel", sHOME) ;
                        break ;
                    case Contact::Address2:
                        item.insert("rel", sOTHER) ;
                        break ;
                    }
                    addresses.append(item) ;
                }
            }
            if (addresses.size()>0) entry.insert("gd$structuredPostalAddress", addresses) ;


            // Phone
            // Phone2
            // Work
            // Mobile
            // Voip
            // Phone2Title
            QJsonArray numbers ;
            for (int i=Contact::Phone; i<=Contact::Voip; i++) {
                QString& phone = contact.getField((Contact::ContactRecord)i) ;
                if (!phone.isEmpty()) {
                    QJsonObject item ;
                    item.insert("$t", phone) ;
                    switch (i) {
                    case Contact::Phone: item.insert("rel", sHOME) ; break ;
                    case Contact::Work: item.insert("rel", sWORK) ; break ;
                    case Contact::Mobile: item.insert("rel", sMOBILE) ; break ;
                    case Contact::Phone2: item.insert("label", contact.getField(Contact::Phone2Title)) ; break ;
                    case Contact::Phone3: item.insert("label", contact.getField(Contact::Phone3Title)) ; break ;
                    case Contact::Phone4: item.insert("label", contact.getField(Contact::Phone4Title)) ; break ;
                    }
                    numbers.append(item) ;
                }
            }
            if (numbers.size()>0) entry.insert("gd$phoneNumber", numbers) ;

            // Email
            // Email2
            QJsonArray emails ;
            for (int i=Contact::Email; i<=Contact::Email2; i++) {
                QString& email = contact.getField((Contact::ContactRecord)i) ;
                if (!email.isEmpty()) {
                    QJsonObject item ;
                    item.insert("address", email) ;
                    switch (i) {
                    case Contact::Email:
                        item.insert("primary", "true") ;
                        if (contact.getField(Contact::Group).compare(gBUSINESS)==0) item.insert("rel", sWORK) ;
                        else item.insert("rel", sHOME) ;
                        break ;
                    case Contact::Email2:
                        item.insert("rel", sOTHER) ;
                        break ;
                    }
                    emails.append(item) ;
                }
            }
            if (emails.size()>0) entry.insert("gd$email", emails) ;


            // Webaddress
            QJsonArray websites ;
            QString& website = contact.getField(Contact::Webaddress) ;
            if (!website.isEmpty()) {
                QJsonObject item ;
                item.insert("href", website) ;
                item.insert("rel", sHOMEPAGE) ;
                websites.append(item) ;
            }
            if (websites.size()>0) entry.insert("gContact$website", websites) ;

            // Birthday
            QString& dob = contact.getField(Contact::Birthday) ;
            if (!dob.isEmpty()) {
                QJsonObject item ;
                item.insert("when", dob) ;
                entry.insert("gContact$birthday", item) ;
            }

            // Comments
            QString& comments = contact.getField(Contact::Comments) ;
            if (!comments.isEmpty()) {
                QJsonObject item ;
                item.insert("$t", comments) ;
                entry.insert("content", item) ;
            }

        }

        // Encoding & Entry
        QJsonDocument doc ;
        QJsonObject root ;
        root.insert("encoding", gConf->getCodec()) ;
        root.insert("entry", entry) ;
        doc.setObject(root) ;
        QString jsontext = doc.toJson() ;

        // createnew = POST, Update = PUT
        QString jsonresponse = googlePutPostDelete(url, action, jsontext, "contactupdate.txt") ;

        // Update the google cached data in the contact record
        if (!getNetworkError().isEmpty()) {
            addLog("ERROR Contact::updateContact: PutPost Returned error " + getNetworkError() + ". " + jsonresponse + "\n") ;
        } else {
            Contact response ;
            QString tag = "entry" ;
            if (parseContact(jsonresponse, tag, response)) {
                success=true ;
                for (int i=Contact::FIRSTSYNCEDRECORD; i<=Contact::LASTSYNCEDRECORD; i++) {
                    QString& responsedata = response.getField((Contact::ContactRecord)i) ;
                    QString& sourcedata = contact.getField((Contact::ContactRecord)i) ;
                    if (sourcedata.compare(responsedata)!=0) {
                        addLog("ERROR Contact::updateContact: Put/Post missing response for: " +
                               contact.getField(Contact::ID) + " (" +
                               response.getContactRecordName((Contact::ContactRecord)i) +
                                "). Sent: '" + sourcedata + "', Received: '" + responsedata + "'\n") ;
                        success=false ;
                    }
                }
                if (success) {
                    response.copyGoogleAccountFieldsTo(contact) ;
                }
            }
        }
    }

    return success ;
}

