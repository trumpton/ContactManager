
#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "configuration.h"

#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUuid>

// Namespace for the local IDs made from Google event IDs (see parseAppointment)
static const QUuid googleEventIdNamespace("6c0f3a52-9b1e-4d7a-8f2e-5a1c7d9e3b40") ;

//=====================================================================================================
//
// Public: GoogleAccess - getCalendar
//
//  Returns a list of all of the contacts on the google contacts
//  account.  Note that this downloads all entries, not just those
//  which have changed since the last sync.
//

bool GoogleAccess::getCalendar(Calendar &googlecalendar, int startdays, int enddays)
{
    QJsonParseError err;
    QJsonDocument doc ;
    QString json ;
    bool success=false ;

    googlecalendar.clear() ;

    QString account = gConf->getGoogleUsername().toHtmlEscaped() ;

    if (refreshtoken.isEmpty()) return false ;

    QString url = "https://www.googleapis.com/calendar/v3/calendars/" +
            account + "/events?v=3.0" ;

    // Full refresh, so get everything that covers dates from startdays ago to enddays
    QDateTime startwindow = QDateTime::currentDateTimeUtc().addDays(-1 * startdays) ;
    QDateTime endwindow = QDateTime::currentDateTimeUtc().addDays(enddays) ;
    QString startwindowstr = dateTimeToIsoString(startwindow) ;
    QString startwindowstrz = dateTimeStringToZulu(startwindowstr) ;
    QString endwindowstr = dateTimeToIsoString(endwindow) ;
    QString endwindowstrz = dateTimeStringToZulu(endwindowstr) ;

    url = url + "&showDeleted=true&showHidden=true&timeMin=" + startwindowstrz + "&timeMax=" + endwindowstrz + "&maxResults=2500" ;


    // Page through the events, following nextPageToken.  Google returns one
    // page at a time (maxResults), so without this later events are missing.
    QString pagetoken ;
    int pagecount=0 ;
    do {
        QString pageurl = url ;
        if (!pagetoken.isEmpty()) pageurl = url + "&pageToken=" + QString(QUrl::toPercentEncoding(pagetoken)) ;

        json = googleGet(pageurl, "getCalendar") ;
        success = errorstatus.isEmpty() ;

        if (!success) return false ;

        doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

        if (err.error != QJsonParseError::NoError) return false ;
        if (doc.isNull()) return false ;
        if (!doc.isObject()) return false ;

        QJsonObject obj = doc.object() ;
        QJsonArray items = obj["items"].toArray() ;

        // get a copy of the QJsonObject

        foreach (const QJsonValue & value, items) {

            if (value.isObject()){
                Appointment appt ;
                QJsonObject item = value.toObject() ;

                // Add the changes, storing only non-deleted entries
                // An exception to a repeat has a recurringEventId: the repeat is the entry, so it is not stored again
                if (item["recurringEventId"].toString().isEmpty() && parseAppointment(item, appt, account)) {
                    if (!appt.isSet(Appointment::Deleted)) {
                        googlecalendar.addAppointment(appt, false) ;
                    }
                }
            }
        }

        pagetoken = obj["nextPageToken"].toString() ;
        pagecount++ ;

    } while (!pagetoken.isEmpty()) ;

    dbg(QString("TIMING getCalendar: %1 page(s), %2 events").arg(pagecount).arg(googlecalendar.size())) ;
    return true ;
}

bool GoogleAccess::getAppointment(Appointment& appt)
{
    QJsonParseError err;
    QJsonDocument doc ;
    QString json ;
    QString account = gConf->getGoogleUsername().toHtmlEscaped() ;
    QString& record = appt.getField(Appointment::GoogleRecordId) ;

    QString url = "https://www.googleapis.com/calendar/v3/calendars/" +
            account + "/events/" +
            record + "?v=3.0" ;

    json = googleGet(url, "getAppointment") ;

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
    QString summary = item["summary"].toString().trimmed()  ;
    QString description = item["description"].toString().trimmed()  ;
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
    QString uploaded = item["extendedProperties"].toObject()["private"].toObject()["Uploaded"].toString().replace("\n","") ;
    QString contact = item["extendedProperties"].toObject()["private"].toObject()["ContactID"].toString().replace("\n","") ;
    bool organiserself = item["organizer"].toObject()["self"].toBool() ;

    // Remove any trailing brackets appointment-for {}, and set description and appointmentfor
    if (!appointmentfor.isEmpty()) {
        summary = summary.trimmed().replace(QRegularExpression("\\{(.*)\\}$"), "").trimmed() ;
    }
    summary = summary.replace(QString("{"),"(").replace(QString("}"),")") ;

    if (!appointmentfor.isEmpty()) {
        appt.setField(Appointment::For, appointmentfor) ;
    } else {
        appt.setField(Appointment::For, QString("Unnamed Contact")) ;
    }

    appt.setFlag(Appointment::Deleted, (status.compare("cancelled")==0) ) ;
    appt.setField(Appointment::Summary, summary) ;
    appt.setField(Appointment::Description, description) ;
    appt.setField(Appointment::Location, location) ;
    appt.setField(Appointment::Flags, flags) ;

    // The rule of a repeating event: a repeat every day, week, month or year.  Other rules are read as the first
    // occurrence only, as the app cannot show them.  Exceptions to a repeat are skipped by the download.
    QString repeattype = "0" ;
    QString repeatevery = "0" ;
    foreach (const QJsonValue &rule, item["recurrence"].toArray()) {
        QString text = rule.toString().trimmed() ;
        if (!text.startsWith("RRULE:")) continue ;
        QString freq ;
        int every = 1 ;
        foreach (const QString &part, text.mid(6).split(';')) {
            QStringList keyvalue = part.split('=') ;
            if (keyvalue.size()!=2) continue ;
            if (keyvalue[0]=="FREQ") freq = keyvalue[1] ;
            if (keyvalue[0]=="INTERVAL") every = qMax(1, keyvalue[1].toInt()) ;
        }
        if (freq=="DAILY") repeattype = "1" ;
        else if (freq=="WEEKLY") repeattype = "2" ;
        else if (freq=="MONTHLY") repeattype = "3" ;
        else if (freq=="YEARLY") repeattype = "4" ;
        if (repeattype!="0") repeatevery = QString::number(every) ;
        break ;
    }
    appt.setField(Appointment::Repeat, repeattype) ;
    appt.setField(Appointment::RepeatInterval, repeatevery) ;

    if (!fromdatetime.isEmpty()) {
        appt.setField(Appointment::From, fromdatetime) ;
        appt.setField(Appointment::To, todatetime) ;
    } else {
        appt.setField(Appointment::From, fromdate) ;
        appt.setField(Appointment::To, todate) ;
    }

    if (!id.isEmpty()) appt.setField(Appointment::ID, id) ;
    appt.setField(Appointment::ContactId, contact) ;

    // Get id, sequence from google.  Account not included in response, so
    // convert to % format for storage (just like contact)
    QString googleicaluid = item["iCalUID"].toString().replace("\n","") ;
    QString googleid = item["id"].toString().replace("\n","") ;

    // An event the app did not upload has no private ID, so the ID set above is a new random one
    // each time the event is downloaded. Derive a stable ID from the Google event ID instead, so the
    // same event keeps the same local ID from one sync to the next
    if (id.isEmpty() && !googleid.isEmpty()) {
        appt.setField(Appointment::ID, QUuid::createUuidV5(googleEventIdNamespace, googleid.toUtf8()).toString(QUuid::WithoutBraces)) ;
    }

    qint64 sequence = item["sequence"].toInt()  ;
    account = account.replace("@", "%40") ;

    if (!(googleid.isEmpty() && googleicaluid.isEmpty())) {
        appt.setField(Appointment::GoogleRecordId, googleid) ;
        appt.setField(Appointment::GoogleSequence, QString::number(sequence)) ;
        appt.setField(Appointment::GoogleCreated, googlecreated) ;
        appt.setFlag(Appointment::InternetOwned, !organiserself) ;
        appt.setField(Appointment::GoogleStatus, status) ;
        if (uploaded.compare("yes")==0) {
            appt.setFlag(Appointment::Uploaded, true) ;
        } else {
            appt.setFlag(Appointment::Uploaded, false) ;
        }
    }

    // Set updated last
    appt.setField(Appointment::Created, googlecreated) ;
    appt.setField(Appointment::DateUpdated, googleupdated) ;
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

    // Read-only Google entries are never sent to Google, not even as a POST: a POST would create
    // a new copy of the entry on every sync
    if (appt.isSet(Appointment::InternetOwned))
        return true ;

    // Get account
    QString account = gConf->getGoogleUsername().toHtmlEscaped() ;

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
        googlePutPostDelete(url, GoogleAccess::Delete, "", "updateAppointment-Delete") ;

        // If successful, update local status to indicate Google Record has been deleted
        if (!isConnectionError()) {
            success=true ;
            appt.setField(Appointment::GoogleStatus, "cancelled") ;
        }

    } else {

        // Put / Post

        if (action==GoogleAccess::Put) {
            root.insert("id", appt.getField(Appointment::GoogleRecordId)) ;
            qint64 sequence = appt.getField(Appointment::GoogleSequence).toLong() ;
            root.insert("sequence", QString::number(sequence+1)) ;
        }

        root.insert("status", "confirmed") ;

        QString summary = appt.getField(Appointment::Summary) ;
        if (!appt.getField(Appointment::ContactId).isEmpty() && appt.getField(Appointment::ContactId).compare(gConf->getMe())!=0) {
            summary = summary + " {" + appt.getField(Appointment::For) + "}" ;
        }

        root.insert("summary",  summary) ;
        root.insert("description",  appt.getField(Appointment::Description)) ;
        root.insert("location", appt.getField(Appointment::Location)) ;

        // A repeat is sent as its rule, so Google repeats it.  An empty list is no repeat.
        QJsonArray rules ;
        if (appt.isRepeating()) {
            static const char *freqs[5] = { "", "DAILY", "WEEKLY", "MONTHLY", "YEARLY" } ;
            int type = appt.getField(Appointment::Repeat).toInt() ;
            int every = qMax(1, appt.getField(Appointment::RepeatInterval).toInt()) ;
            rules.append("RRULE:FREQ=" + QString(freqs[type]) + ";INTERVAL=" + QString::number(every)) ;
        }
        root.insert("recurrence", rules) ;

        if (false) {
            // All Day Event
        } else {
            // Normal Event
            QJsonObject StartDateTime ;
            StartDateTime.insert("dateTime", appt.getField(Appointment::From)) ;
            QJsonObject EndDateTime ;
            EndDateTime.insert("dateTime", appt.getField(Appointment::To)) ;

            // A repeating event needs a time zone on its start and end, or Google refuses it
            if (appt.isRepeating()) {
                StartDateTime.insert("timeZone", "UTC") ;
                EndDateTime.insert("timeZone", "UTC") ;
            }

            root.insert("start", StartDateTime) ;
            root.insert("end", EndDateTime) ;
        }

        QJsonObject ExtendedProperties ;
        QJsonObject PrivateExtendedProperties ;
        PrivateExtendedProperties.insert("ID", appt.getField(Appointment::ID)) ;
        PrivateExtendedProperties.insert("ContactID", appt.getField(Appointment::ContactId)) ;
        PrivateExtendedProperties.insert("AppointmentFor", appt.getField(Appointment::For)) ;
        PrivateExtendedProperties.insert("Uploaded", "yes") ;
        PrivateExtendedProperties.insert("Flags", appt.getField(Appointment::Flags)) ;

        ExtendedProperties.insert("private", PrivateExtendedProperties) ;
        root.insert("extendedProperties", ExtendedProperties) ;

        doc.setObject(root) ;
        QString jsontext = doc.toJson() ;

        // createnew = POST, Update = PUT
        jsonresponse = googlePutPostDelete(url, action, jsontext, "updateAppointment-PutPost") ;

    }

    // Update the local record with the Google data
    if (!getNetworkError().isEmpty()) {
        addLog("ERROR GoogleAccess::UpdateAppointment: Returned error " + getNetworkError() + ". " + jsonresponse + "\n") ;
    } else {

        Appointment apptresponse ;
        if (parseAppointment(jsonresponse, apptresponse, account)) {

            if (apptresponse.matches(appt, Appointment::maDetails|Appointment::maId)) {
                apptresponse.copyTo(appt, Appointment::maDetails|Appointment::maGoogleAcct) ;
                success=true ;

            } else {
                // Returned info doesn't match sent
                addLog(QString("ERROR GoogleAccess::UpdateAppointment: Upload mismatch (%1) - %2")
                       .arg(appt.getField(Appointment::ID))
                       .arg(apptresponse.mismatch(appt, Appointment::maDetails|Appointment::maId, true))) ;
                success=false ;
            }
        }
    }

    return success ;
}


