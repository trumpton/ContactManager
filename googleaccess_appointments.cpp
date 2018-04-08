
#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "configuration.h"

#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>

//=====================================================================================================
//
// Public: GoogleAccess - getCalendar
//
//  Returns a list of all of the contacts on the google contacts
//  account.
//

bool GoogleAccess::getCalendar(Calendar &googlecalendar)
{
    QJsonParseError err;
    QJsonDocument doc ;
    QString json ;

    googlecalendar.clear() ;

    QString account = gConf->getGoogleUsername();

    if (refreshtoken.isEmpty()) return false ;

    QString url = "https://www.googleapis.com/calendar/v3/calendars/" +
            account.toHtmlEscaped() + "/events?v=3.0" ;

    if (calendarsynctoken.isEmpty()) {

        // Full refresh, so get everything that covers dates from 8 months ago
        QDateTime startwindow = QDateTime::currentDateTimeUtc().addMonths(-18) ;
        QString startwindowstr = dateTimeToIsoString(startwindow) ;
        QString startwindowstrz = dateTimeStringToZulu(startwindowstr) ;
        url = url + "&showDeleted=true&showHidden=true&timeMin=" + startwindowstrz ;

    } else {

        // Just get what has changed since the last successful call
        url = url + QString("&syncToken=") + calendarsynctoken ;

    }

    json = googleGet(url, "appointmentlist-1.txt") ;
    if (!errorstatus.isEmpty()) return false ;

    doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

    if (err.error != QJsonParseError::NoError) return false ;
    if (doc.isNull()) return false ;
    if (!doc.isObject()) return false ;

    QJsonObject obj = doc.object() ;
    QJsonArray items = obj["items"].toArray() ;

    // Retrieve the sync token
    QString token = obj["nextSyncToken"].toString() ;
    if (token.isEmpty()) return false ;
    calendarsynctoken = token ;

    // get a copy of the QJsonObject

    foreach (const QJsonValue & value, items) {

        if (value.isObject()){
            Appointment appt ;
            QJsonObject item = value.toObject() ;

            // Add the changes
            if (parseAppointment(item, appt, account)) {                
                    googlecalendar.addAppointment(appt, false) ;
                }
        }
    }
    return true ;
}

bool GoogleAccess::getAppointment(Appointment& appt)
{
    QJsonParseError err;
    QJsonDocument doc ;
    QString json ;
    QString& account = appt.getField(Appointment::GoogleAccount) ;
    QString& record = appt.getField(Appointment::GoogleRecordId) ;

    QString url = "https://www.googleapis.com/calendar/v3/calendars/" +
            account + "/events/" +
            record + "?v=3.0" ;

    json = googleGet(url, "appointment.txt") ;

    if (!errorstatus.isEmpty()) return false ;

    doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

    if (err.error != QJsonParseError::NoError) return false ;
    if (doc.isNull()) return false ;
    if (!doc.isObject()) return false ;

    QJsonObject obj = doc.object() ;

    Appointment newappt ;
    if (parseAppointment(obj, newappt, account)) {
         appt=newappt ;
    }
    return true ;
}


//=====================================================================================================
//
// Private: GoogleAccess - parseAppointment
//
//  Parse a single JSonObject into an appointment
//

bool GoogleAccess::parseAppointment(QJsonObject &item, Appointment &appt, QString account)
{
    QString description = item["summary"].toString().trimmed()  ;
    QString location = item["location"].toString().trimmed()  ;
    QString fromdatetime = item["start"].toObject()["dateTime"].toString().trimmed()  ;
    QString todatetime = item["end"].toObject()["dateTime"].toString().trimmed()  ;
    QString fromdate = item["start"].toObject()["date"].toString().trimmed()  ;
    QString todate = item["end"].toObject()["date"].toString().trimmed()  ;
    QString status = item["status"].toString().trimmed() ;
    QString googleupdated = item["updated"].toString().trimmed() ;
    QString googlecreated = item["created"].toString().trimmed() ;
    QString appointmentfor = item["extendedProperties"].toObject()["private"].toObject()["AppointmentFor"].toString().trimmed() ;
    QString id = item["extendedProperties"].toObject()["private"].toObject()["ID"].toString().replace("\n","") ;
    QString flags = item["extendedProperties"].toObject()["private"].toObject()["Flags"].toString().replace("\n","") ;
    QString contact = item["extendedProperties"].toObject()["private"].toObject()["ContactID"].toString().replace("\n","") ;
    bool organiserself = item["organizer"].toObject()["self"].toBool() ;

    // TODO: Appointment repeats are missing

    // Remove any trailing brackets appointment-for {}, and set description and appointmentfor
    if (!appointmentfor.isEmpty()) {
        description = description.trimmed().replace(QRegExp("\\{(.*)\\}$"), "").trimmed() ;
    }
    description = description.replace(QString("{"),"(").replace(QString("}"),")") ;

    if (!appointmentfor.isEmpty()) {
        appt.setField(Appointment::For, appointmentfor) ;
    } else {
        appt.setField(Appointment::For, QString("Unnamed Contact")) ;
    }

    appt.setFlag(Appointment::Deleted, (status.compare("cancelled")==0) ) ;
    appt.setField(Appointment::Description, description) ;
    appt.setField(Appointment::Location, location) ;
    appt.setField(Appointment::Flags, flags) ;

    if (!fromdatetime.isEmpty()) {
        appt.setField(Appointment::From, fromdatetime) ;
        appt.setField(Appointment::To, todatetime) ;
    } else {
        appt.setField(Appointment::From, fromdate) ;
        appt.setField(Appointment::To, todate) ;
    }

    //TODO: Process and extract Repeat and RepeatInterval

    if (!id.isEmpty()) appt.setField(Appointment::ID, id) ;
    appt.setField(Appointment::ContactId, contact) ;

    // Get id, sequence from google.  Account not included in response, so
    // convert to % format for storage (just like contact)
    QString googleicaluid = item["iCalUID"].toString().replace("\n","") ;
    QString googleid = item["id"].toString().replace("\n","") ;

    qint64 sequence = item["sequence"].toInt()  ;
    account = account.replace("@", "%40") ;

    if (!(googleid.isEmpty() && googleicaluid.isEmpty())) {
        appt.setField(Appointment::GoogleRecordId, googleid) ;
        appt.setField( Appointment::GoogleIcalUid, googleicaluid) ;
        appt.setField(Appointment::GoogleAccount, account) ;
        appt.setField(Appointment::GoogleSequence, QString::number(sequence)) ;
        appt.setField(Appointment::GoogleCreated, googlecreated) ;
        appt.setFlag(Appointment::InternetOwned, !organiserself) ;
        appt.setField(Appointment::GoogleStatus, status) ;
    }

    // Set updated last
    appt.setField(Appointment::Created, googlecreated) ;
    appt.setField(Appointment::FromUpdated, googleupdated) ;
    appt.setField(Appointment::Updated, googleupdated) ;

    return (!googleid.isEmpty()) ;
}

bool GoogleAccess::parseAppointment(QString &json, Appointment &appt, QString account)
{
    QJsonParseError err;
    QJsonDocument doc ;
    doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

    if (err.error != QJsonParseError::NoError) return false ;
    if (doc.isNull()) return false ;
    if (!doc.isObject()) return false ;

    QJsonObject jsonobject = doc.object() ;
    return parseAppointment(jsonobject, appt, account);
}

//=====================================================================================================
//
// Public: GoogleAccess - updateAppointment
//
//  Update the appointment on the Google server
//
bool GoogleAccess::updateAppointment(Appointment &appt, googleAction action)
{
    bool success=false ;
    QJsonDocument doc ;
    QJsonObject root ;
    QString url ;
    QString jsonresponse ;

    // Override action for deletions
    if (appt.isSet(Appointment::Deleted))
        action = GoogleAccess::Delete ;

    // TODO: need a private flag or flip sense of googleorganiseself to googleothercalendar
    // Prevent read-only Google entries being changed (should never happen as entry is readonly)
    if (action!=GoogleAccess::Create && appt.isSet(Appointment::InternetOwned))
        return true ;

    // Get account
    QString account = gConf->getGoogleUsername() ;
    // TODO: QUrl::toPercentEncoding
    account = account.replace("@", "%40") ;

    // Calculate URL
    if (action==GoogleAccess::Delete) {
        QString googlerecordid = appt.getField(Appointment::GoogleRecordId) ;
        url = "https://www.googleapis.com/calendar/v3/calendars/" + account + "/events/" + googlerecordid ;
    } else if (action==GoogleAccess::Post) {
        url = "https://www.googleapis.com/calendar/v3/calendars/" + account + "/events" ;
    } else if (action==GoogleAccess::Put) {
        QString googlerecordid = appt.getField(Appointment::GoogleRecordId) ;
        url = "https://www.googleapis.com/calendar/v3/calendars/" + account + "/events/" + googlerecordid ;
        root.insert("iCalUID", googlerecordid) ;
    } else {
        // Error
    }

    // Process Request
    if (action==GoogleAccess::Delete) {

        // Delete
        googlePutPostDelete(url, GoogleAccess::Delete, "", "appointmentupdate.txt") ;

        // If successful, update local status to indicate Google Record has been deleted
        if (!isConnectionError()) {
            success=true ;
            appt.setField(Appointment::GoogleStatus, "cancelled") ;
        }

    } else {

        // Put / Post

        // TODO: iCalUID may have @ / %40 translation requirements here and in parse
        if (action==GoogleAccess::Update) {
            root.insert("id", appt.getField(Appointment::GoogleRecordId)) ;
            root.insert("iCalUID", appt.getField(Appointment::GoogleIcalUid)) ;
            qint64 sequence = appt.getField(Appointment::GoogleSequence).toLong() ;
            root.insert("sequence", QString::number(sequence+1)) ;
        }

        // TODO: Don't need to store the sequence, as on success, it is extracted from the response
        //appt.setField(Appointment::GoogleSequence, QString::number(sequence+1), Appointment::GAccount) ;

        // TODO: Set Repeat / RepeatInterval

        root.insert("status", "confirmed") ;

        QString summary = appt.getField(Appointment::Description) ;
        if (!appt.getField(Appointment::ContactId).isEmpty() && appt.getField(Appointment::ContactId).compare(gConf->getMe())!=0) {
            summary = summary + " {" + appt.getField(Appointment::For) + "}" ;
        }

        root.insert("summary",  summary) ;
        root.insert("location", appt.getField(Appointment::Location)) ;

        if (false) {
            // All Day Event
        } else {
            // Normal Event
            QJsonObject StartDateTime ;
            StartDateTime.insert("dateTime", appt.getField(Appointment::From)) ;
            root.insert("start", StartDateTime) ;
            QJsonObject EndDateTime ;
            EndDateTime.insert("dateTime", appt.getField(Appointment::To)) ;
            root.insert("end", EndDateTime) ;
        }

        QJsonObject ExtendedProperties ;
        QJsonObject PrivateExtendedProperties ;
        PrivateExtendedProperties.insert("ID", appt.getField(Appointment::ID)) ;
        PrivateExtendedProperties.insert("ContactID", appt.getField(Appointment::ContactId)) ;
        PrivateExtendedProperties.insert("AppointmentFor", appt.getField(Appointment::For)) ;
        PrivateExtendedProperties.insert("Flags", appt.getField(Appointment::Flags)) ;

        ExtendedProperties.insert("private", PrivateExtendedProperties) ;
        root.insert("extendedProperties", ExtendedProperties) ;

        // TODO: also set google save fields (to be same as appt fields)

        doc.setObject(root) ;
        QString jsontext = doc.toJson() ;

        // createnew = POST, Update = PUT
        jsonresponse = googlePutPostDelete(url, action, jsontext, "appointmentupdate.txt") ;

    }

    // Update the local record with the Google data
    if (!getNetworkError().isEmpty()) {
        addLog("ERROR GoogleAccess::UpdateAppointment: Returned error " + getNetworkError() + ". " + jsonresponse + "\n") ;
    } else {

        Appointment apptresponse ;
        if (parseAppointment(jsonresponse, apptresponse, account)) {
            apptresponse.copySyncedFieldsTo(appt) ;
            apptresponse.copyGoogleAccountFieldsTo(appt) ;
            success=true ;
        }
    }

    return success ;
}


