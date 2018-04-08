#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "configuration.h"
#include <QUrl>

#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>


//=====================================================================================================
//
// Public: GoogleAccess - loadGroups
//
//  Load all of the google group types
//
//  Return true if the sGroupMyContacts group is successfuly set
//

bool GoogleAccess::loadGroups()
{
    if (refreshtoken.isEmpty()) return false ;

    QString json = googleGet("https://www.google.com/m8/feeds/groups/default/full?v=3.0&alt=json&max-results=9999", "groupslist.txt") ;

    if (!errorstatus.isEmpty()) return false ;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

    if (err.error != QJsonParseError::NoError) return false ;
    if (doc.isNull()) return false ;
    if (!doc.isObject()) return false ;

    QJsonObject obj = doc.object() ;
    QJsonArray items = obj["feed"].toObject()["entry"].toArray() ;

    groupslist.clear() ;

    foreach (const QJsonValue & value, items) {

        if (value.isObject()){
            QJsonObject item = value.toObject() ;
            QString id = item["id"].toObject()["$t"].toString() ;
            QString content = item["content"].toObject()["$t"].toString() ;
            content.replace("System Group: ", "") ;
            groupslist.push_back(id + QString("\n") + content) ;

        }
    }

    groupslist.sort() ;
    return (groupslist.size()>0) ;
}

// BUG BUG BUG
// Currently, everything comes back as Hidden and Unknown group

QString& GoogleAccess::findMyContactsId()
{
    return findGroupId("My Contacts") ;
}

//=====================================================================================================
//
// Public: GoogleAccess - findGroupName
//
// Returns the name of the group, given the id
//
QString &GoogleAccess::findGroupName(QString id)
{
    groupfindresult = "" ;
    int i=0 ;
    while (i<groupslist.size() && groupfindresult.isEmpty()) {
        QString entry = groupslist.at(i) ;
        QStringList bits = entry.split(QChar('\n')) ;
        if (id.compare(bits.at(0))==0) groupfindresult=bits.at(1) ;
        i++ ;
    }
    if (groupfindresult.isEmpty()) groupfindresult = "Unknown" ;
    return groupfindresult ;
}

//=====================================================================================================
//
// Public: GoogleAccess - findGroupId
//
// Returns the id of the group, given the name
//
QString &GoogleAccess::findGroupId(QString name)
{
    groupfindresult = "" ;
    int i=0 ;
    while (i<groupslist.size() && groupfindresult.isEmpty()) {
        QString entry = groupslist.at(i) ;
        QStringList bits = entry.split(QChar('\n')) ;
        if (name.compare(bits.at(1))==0) groupfindresult=bits.at(0) ;
        i++ ;
    }
    return groupfindresult ;
}





/*
  THIS IS THE OLD WAY OF STORING GROUPS

            if (content.compare("System Group: My Contacts", Qt::CaseInsensitive)==0) sGroupMyContacts = id ;
            else if (content.compare("Client", Qt::CaseInsensitive)==0) sGroupClient = id ;
            else if (content.compare("Business", Qt::CaseInsensitive)==0) sGroupBusiness = id ;
            else if (content.compare("System Group: Friends", Qt::CaseInsensitive)==0) sGroupFriends = id ;
            else if (sGroupFriends.isEmpty() && content.compare("Friends", Qt::CaseInsensitive)==0) sGroupFriends = id ;
            else if (sGroupFriends.isEmpty() && content.compare("Friend", Qt::CaseInsensitive)==0) sGroupFriends = id ;
            else if (content.compare("System Group: Family", Qt::CaseInsensitive)==0) sGroupFamily = id ;
            else if (sGroupFamily.isEmpty() && content.compare("Family", Qt::CaseInsensitive)==0) sGroupFamily = id ;
            else if (content.compare("Other", Qt::CaseInsensitive)==0) sGroupOther = id ;
            else if (sGroupOther.isEmpty() && content.compare("System Group: Coworkers", Qt::CaseInsensitive)==0) sGroupOther = id ;
            else if (sGroupOther.isEmpty() && content.compare("Colleague", Qt::CaseInsensitive)==0) sGroupOther = id ;
            else if (sGroupOther.isEmpty() && content.compare("Coworkers", Qt::CaseInsensitive)==0) sGroupOther = id ;
            else if (sGroupOther.isEmpty() && content.compare("Work", Qt::CaseInsensitive)==0) sGroupOther = id ;

*/
