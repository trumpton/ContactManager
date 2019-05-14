//=====================================================================================================
//
// Public: GoogleAccess - updateGoogleContactGroup / getGroupIds
//
// DESCRIPTION:
//
//  Retrieves the contact groups list (getGroupIds)
//  Adds / Removes contact from groups
//
// PARAMETERS:
//
//    contact         -   The contact structure to use in the update
//    googlecontact   -   The current google contact structure (to enable required changes to be identified)
//    pass            -   The pass number (for logging when performing multiple attempts)
//
// API CALLS:
//
//    https://people.googleapis.com/v1/contactGroups/ID/members:modify (GoogleAccess::Post)
//    https://people.googleapis.com/v1/contactGroups (Get)
//    https://people.googleapis.com/v1/contactGroups (Post)
//

#include "googleaccess.h"
#include "../Lib/supportfunctions.h"

#include <QUrl>
#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>

QString GoogleAccess::getGroupId(Contact::ContactRecord rec, bool asverbosename)
{
    QString reply="" ;
    if ((Contact::ContactRecord)rec==Contact::GroupBusiness) { reply = (asverbosename?"Business":gidbusiness) ; }
    else if ((Contact::ContactRecord)rec==Contact::GroupClient) { reply = (asverbosename?"Client":gidclient) ; }
    else if ((Contact::ContactRecord)rec==Contact::GroupFamily) { reply = (asverbosename?"My Family":gidfamily) ; }
    else if ((Contact::ContactRecord)rec==Contact::GroupFriend) { reply = (asverbosename?"My Friends":gidfriend) ; }
    else { reply = (asverbosename?"Other":gidother) ; }
    return reply ;
}

bool GoogleAccess::getGroupIds()
{

    QString url="https://people.googleapis.com/v1/contactGroups" ;

    QString json = googleGet(url, QString("getGroupIds")) ;
    if (!getNetworkError().isEmpty()) {
        addLog("GoogleAccess::getGroupIds: Error fetching list of groups") ;
    } else {
        QJsonDocument doc ;
        QJsonParseError err ;

        doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

        if (err.error != QJsonParseError::NoError) return false ;
        if (doc.isNull()) return false ;
        if (!doc.isObject()) return false ;

        QJsonObject obj = doc.object() ;

        addLog("GoogleAccess::getGroupIds: Loaded groups:") ;

        QJsonArray groupsarray = obj["contactGroups"].toArray() ;
        foreach (const QJsonValue& groupitem, groupsarray) {
            if (groupitem.isObject()) {
                QString resourceName = groupitem.toObject()["resourceName"].toString().replace("contactGroups/","") ;
                QString name = groupitem.toObject()["name"].toString().toLower().replace(" ","") ;
                addLog(QString("    ") + name + QString(", ") + resourceName) ;
                if (name.compare(getGroupId(Contact::GroupBusiness, true).toLower().replace(" ",""))==0) gidbusiness=resourceName ;
                if (name.compare(getGroupId(Contact::GroupClient, true).toLower().replace(" ",""))==0) gidclient=resourceName ;
                if (name.compare(getGroupId(Contact::GroupFamily, true).toLower().replace(" ",""))==0) gidfamily=resourceName ;
                if (name.compare(getGroupId(Contact::GroupFriend, true).toLower().replace(" ",""))==0) gidfriend=resourceName ;
                if (name.compare(getGroupId(Contact::GroupOther, true).toLower().replace(" ",""))==0) gidother=resourceName ;
            }            
        }
    }
    if (gidbusiness.isEmpty() || gidclient.isEmpty() || gidfamily.isEmpty() ||
            gidfriend.isEmpty() || gidother.isEmpty()) {
        return false ;
    } else {
        return true ;
    }
}

bool GoogleAccess::createGroupIds()
{
    bool success=true ;
    QString groupurl = "https://people.googleapis.com/v1/contactGroups" ;

    for (int i=Contact::GroupBusiness; i<=Contact::GroupOther; i++) {

        if (getGroupId((Contact::ContactRecord)i).isEmpty()) {

            QString groupdesc = getGroupId((Contact::ContactRecord)i, true) ;

            // {
            //  "contactGroup": {
            //    "name": "API Group"
            //  }

            QJsonDocument doc ;
            QJsonObject entry, group ;

            group.insert("name", groupdesc) ;
            entry.insert("contactGroup", group) ;
            doc.setObject(entry) ;

            // Create the Group
            QString jsonresponse ;
            jsonresponse = googlePutPostDelete(groupurl, GoogleAccess::Post, doc.toJson(), QString("createGroupId-" + groupdesc.replace(" ","").toLower())) ;
            if (!getNetworkError().isEmpty()) {
                success=false ;
                addLog("GoogleAccess::createGroupIds: ERROR - " + getNetworkError() + ".\n") ;
            }

        }

    }

    return success ;
}

bool GoogleAccess::updateSingleGoogleContactGroup(Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb)
{
    bool success=true ;
    QString updateurl = "https://people.googleapis.com/v1/contactGroups/" + getGroupId(rec, false) + "/members:modify" ;
    int added=0, deleted=0 ;
    QJsonObject entry ;
    QJsonArray addresources, deleteresources ;

    addedstr="" ;
    deletedstr="" ;

    for (int i=0; i<googledb.size(); i++) {

        Contact& googlecontact = googledb.getContact(i) ;
        Contact& localcontact = db.getContactById(googlecontact.getField(Contact::ID)) ;

        // Do transfer if valid entry and entry exists on google, and transfer requested
        bool dotransfer = true ;
        if (localcontact.isNull()) dotransfer=false ;
        if (googlecontact.getField(Contact::GoogleRecordId).isEmpty()) dotransfer=false ;
        if (!localcontact.isSet(Contact::ToBeUploaded)) dotransfer=false ;

        if (dotransfer) {

            bool googlegrp = googlecontact.isSet(rec) ;
            bool localgrp = localcontact.isSet(rec) ;

            if (googlegrp!=localgrp) {
                if (localgrp) {
                    addresources.append(googlecontact.getField(Contact::GoogleRecordId)) ;
                    if (!addedstr.isEmpty()) addedstr = addedstr + "\n" ;
                    addedstr = addedstr + "   + " + googlecontact.getFormattedName(false, false) ;
                    added++ ;
                } else {
                    deleteresources.append(googlecontact.getField(Contact::GoogleRecordId)) ;
                    if (!deletedstr.isEmpty()) deletedstr = deletedstr + "\n";
                    deletedstr = deletedstr + "   - " + googlecontact.getFormattedName(false, false) ;
                    deleted++ ;
                }
            }
        }
    }

    // { "resourceNamesToAdd": [ string ],
    //   "resourceNamesToRemove": [ string ] }
    if (added>0) entry.insert("resourceNamesToAdd", addresources) ;
    if (deleted>0) entry.insert("resourceNamesToRemove", deleteresources) ;

    if (added!=0 || deleted!=0){

        // Do the Google update
        QJsonDocument doc ;
        QString jsonresponse ;

        doc.setObject(entry) ;
        jsonresponse = googlePutPostDelete(updateurl, GoogleAccess::Post, doc.toJson(), QString("updateSingleGoogleContactGroup-" + getGroupId(rec,true).replace(" ","").toLower())) ;

        if (!getNetworkError().isEmpty()) {

            success=false ;
            addLog("GoogleAccess::updateSingleGoogleContactGroup: ERROR - " + getNetworkError() + ".\n") ;

        } else {

            addLog("GoogleAccess::updateSingleGoogleContactGroup: Updated Google Group: " + getGroupId(rec, true) + QString(" - %1 added, %2 removed.").arg(added).arg(deleted)) ;

            // Copy the flags across for everything that was successfully changed
            for (int j=0; j<googledb.size(); j++) {
                Contact& googlecontact = googledb.getContact(j) ;
                Contact& localcontact = db.getContactById(googlecontact.getField(Contact::ID)) ;
                if (!localcontact.isNull()) { googlecontact.setFlag(rec, localcontact.isSet(rec)) ; }
            }

            // This should not be necessary, but is required as only the last group update is used
            QThread::msleep(8000) ;

        }
    }

    return success ;
}

QString& GoogleAccess::contactsDeletedFromGroup()
{
    return addedstr ;
}

QString& GoogleAccess::contactsAddedToGroup()
{
    return deletedstr ;
}
