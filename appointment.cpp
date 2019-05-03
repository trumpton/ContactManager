#include "appointment.h"
#include <QDateTime>
#include <QUuid>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QTextDecoder>
#include "../Lib/supportfunctions.h"

// TODO: Need to think about how recurring appointments are handled, perhaps getFromDate and getToDate can take an occurrence parameter?

// TODO: Always work with dates in UTC
// Bug Observation: QDateTime.SetDate() Only works if the QDateTime item is set to Utc.
// e.g. qDateTimeGmt.setDate(QDate::CurrentDate()) sets the qDateTimeGmt to "invalid"

Appointment::Appointment()
{
    isnull=false ;
    createNew() ;
    asTextResponse = "" ;
    asHTMLResponse = "" ;
    getDateResponse.setMSecsSinceEpoch(0);
}

Appointment::Appointment(const Appointment &other)
{
    qFatal("FATAL: Appointment copy not supported") ;
}

// forces record to be a read-only null one
void Appointment::setNull()
{
    createNew() ;
    filedata[ID] = "00000000-0000-0000-0000-000000000000" ;
    isnull=true ;
}


Appointment::~Appointment()
{
    if (isdirty) {
        // TODO: Error - Save should be called before destructor
    }
}

bool Appointment::isNull()
{
    return isnull ;
}

bool Appointment::isEmpty()
{
    return isempty ;
}

bool Appointment::isDirty()
{
    return isdirty ;
}

void Appointment::markAsSaved()
{
    isdirty=false ;
}

void Appointment::markAsDirty()
{
    isdirty=true ;
}

QString& Appointment::getId()
{
    return filedata[Appointment::ID] ;
}

QString& Appointment::getContactId()
{
    return filedata[Appointment::ContactId] ;
}

void Appointment::setContactId(QString id)
{
    setField(Appointment::ContactId, id) ;
}

QString& Appointment::getGoogleRecordId()
{
    return filedata[Appointment::GoogleRecordId] ;
}

bool Appointment::load(QString path, QString idname)
{
    if (isnull) {

        return false ;

    } else {

        QString Line ;
        QStringList ParsedLine;
        QRegExp sep("(\\=)");

        createNew() ;
        isdirty = false ;
        QString fullPath = path + idname + ".appointment" ;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream in(&file);
        in.setCodec("UTF-8") ;

        while (!in.atEnd()) {
            Line = in.readLine();
            ParsedLine =  Line.split(sep);

            // TODO: Handle cases where there is an "=" in the middle of the string

            if (ParsedLine.count()==2) {
                for (int i=FIRSTRECORD; i<=LASTRECORD; i++) {
                    if (ParsedLine.at(0).compare(getAppointmentRecordName((enum AppointmentRecord)i))==0) {
                        QString entrytext = ParsedLine.at(1) ;
                        entrytext = entrytext.replace("\\n","\n") ;
                        setField((enum AppointmentRecord)i, entrytext) ;
                    }
                }
            }
        }
        file.close() ;

        isdirty = false ;
        return true ;

    }
}

bool Appointment::save(QString path)
{
    if (isdirty && !isempty && !isnull) {

       QString fullPath = path + filedata[Appointment::ID] + ".appointment" ;
       QFile file(fullPath);
       if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
           return false;
       QTextStream out(&file);
       out.setCodec("UTF-8") ;

       out << "[appointment]\n" ;

       for (int entry=FIRSTRECORD ; entry <= LASTRECORD; entry++) {
           QString entrytext = getField((enum Appointment::AppointmentRecord) entry).replace("\n","\\n") ;
           out << getAppointmentRecordName((enum AppointmentRecord)entry) << "=" << entrytext.trimmed() << "\n" ;
       }

       file.close() ;
       isdirty = false ;
    }
    return true ;
}


char *Appointment::getAppointmentRecordName(enum Appointment::AppointmentRecord field) {
    switch (field) {
        case Appointment::ID: return (char *)"id" ; break ;
        case Appointment::ContactId: return (char *)"contactid" ; break ;
        case Appointment::For: return (char *)"for" ; break ;
        case Appointment::Description: return  (char *)"description" ; break ;
        case Appointment::Location: return  (char *)"location" ; break ;
        case Appointment::From: return  (char *)"from" ; break ;
        case Appointment::To: return  (char *)"to" ; break ;
        case Appointment::Repeat: return  (char *)"repeat" ; break ;
        case Appointment::RepeatInterval: return (char *)"repeatinterval" ; break ;
        case Appointment::Flags: return  (char *)"flags" ; break ;
        case Appointment::Deleted: return (char *)"deleted" ; break ;
        case Appointment::FromUpdated: return (char *)"fromupdated" ; break ;
        case Appointment::MessageSent: return (char *)"messagesent" ; break ;
        case Appointment::ReminderMessageSent: return (char *)"remindermessagesent" ; break ;
        case Appointment::Created: return (char *)"created" ; break ;
        case Appointment::Temporary: return (char *)"istemporary" ; break ;
        case Appointment::InternetOwned: return (char *)"isreadonly" ; break ;
        case Appointment::Updated: return (char *)"updated" ; break ;
        case Appointment::GoogleAccount: return (char *)"googleaccount" ; break ;
        case Appointment::GoogleRecordId: return (char *)"googlerecordid" ; break ;
        case Appointment::GoogleIcalUid: return (char *)"googleicaluid" ; break ;
        case Appointment::GoogleSequence: return (char *)"googlesequence" ; break ;
        case Appointment::GoogleCreated: return (char *)"googlecreated" ; break ;
        case Appointment::GoogleStatus: return (char *)"googlestatus" ; break ;
        default: return  (char *)"INVALID-RECORD-TYPE" ; break ;
    }
}

bool Appointment::isForAccount(QString googleaccount)
{
    return (getField(Appointment::GoogleAccount).compare(googleaccount)==0) ;
}

bool Appointment::isSet(enum Appointment::AppointmentRecord field) const
{
    if (field>=FIRSTRECORD && field<=LASTRECORD &&
            filedata[field].compare("true")==0) return true ;
    return false;
}


void Appointment::setFlag(enum Appointment::AppointmentRecord field, bool flag)
{
    if (flag)  {
        setField(field, "true") ;
    }else {
        setField(field, "false") ;
    }
}


void Appointment::setDate(enum AppointmentRecord field, QDateTime data)
{
    QString datestr = dateTimeToIsoString(data) ;
    setField(field, datestr) ;
}

QDateTime& Appointment::getDate(enum AppointmentRecord field) const
{
    static QDateTime dt ;
    QString data = getField(field) ;
    dt = isoStringToDateTime(data) ;
    return dt ;
}

QString& Appointment::getField(enum Appointment::AppointmentRecord field) const
{
    static QString nullstring ;
    if (field>=FIRSTRECORD && field<=LASTRECORD) { return (QString&)filedata[field] ; }
    return nullstring ;
}


void Appointment::setField(enum Appointment::AppointmentRecord field, QString data)
{
    if (isnull) return ;

    QString dat ;
    dat = data.replace("\r"," ").trimmed() ;

    //
    // Safety checks for poor programming
    //

    if (field<0 || field>Appointment::LASTRECORD) {
        warningOkDialog(0, "ERROR", "Appointment set FILE field invalid", true) ;
        return ;
    }

    //
    // Correct / normalise time entries
    //

    if (!dat.isEmpty() &&
        (  field == Appointment::From || field == Appointment::To ||
          field == Appointment::Created || field == Appointment::Updated ) )
    {
        dat = dateTimeStringToZulu(dat) ;
    }

    //
    // Set field data, and update dirty / empty flags as required
    //

    if (field>=FIRSTRECORD && field<=LASTRECORD &&
            dat.compare(filedata[field])!=0) {

        // Set updated and fromupdated times
        if (field<=Appointment::LASTSYNCEDRECORD) {
            QString now = nowToIsoString() ;
            filedata[Appointment::Updated] = now ;
            if (field==Appointment::From) filedata[Appointment::FromUpdated] = now ;
        }

        // Now set the data
        filedata[field]=dat ;
        isempty = false ;
        isdirty = true ;
    }

    // TODO: This line is for debug purposes, so the summary can be seen in the debugger
    asText() ;

    isnull = false ;
}


void Appointment::createNew()
{
    if (isnull) return ;

    for (int i=(int)FIRSTRECORD; i<=(int)LASTRECORD; i++) {
        setField((enum Appointment::AppointmentRecord)i, "") ;
    }

    QUuid qid = QUuid::createUuid() ;
    if (qid.isNull()) {
        // Error
    }
    QString id=qid.toString();
    id.replace("{","") ;
    id.replace("}","") ;
    setField(Appointment::ID, id) ;

    // Set Repeat
    setField(Appointment::Repeat, "0") ;
    setField(Appointment::RepeatInterval, "0") ;

    // Set a default date/time to 1st January 1970
    QDateTime t = QDateTime::fromTime_t(0) ;
    t.setTimeSpec(Qt::UTC) ;
    setDate(Appointment::From, t) ;
    setDate(Appointment::To, t) ;
    setDate(Appointment::Updated, t) ;

    // Set default dates
    setField(Appointment::Created, nowToIsoString()) ;
    setField(Appointment::MessageSent, QString("")) ;
    setField(Appointment::ReminderMessageSent, QString("")) ;

    // Set flags
    setFlag(Appointment::Deleted, false) ;
    setFlag(Appointment::Temporary, false) ;
    setFlag(Appointment::InternetOwned, false) ;

    isempty = true ;
    isnull = false ;
    isdirty = false ;
}


QString& Appointment::asText(QString name, QString start, QString mid, QString end)
{
    QDateTime from = getDate(Appointment::From) ;
    QDateTime to = getDate(Appointment::To) ;

    if (getField(Appointment::From).isEmpty()) {

        asTextResponse = start + "(purged record)" + end ;

    } else {

        // TODO: parse from/to
        asTextResponse = start ;
        asTextResponse += from.toLocalTime().toString("dd MMM yy hh:mm") ;
        asTextResponse += " - " ;
        if (from == to) {
            asTextResponse += "reminder: " ;
        } else {
            if (from.date() == to.date()) {
                asTextResponse+= to.toLocalTime().toString("hh:mm") + ": " ;
            } else {
                asTextResponse += to.toLocalTime().toString(" dd MMM yy hh:mm") + ": " ;
            }
        }
        if (!name.isEmpty()) {
            asTextResponse = asTextResponse + name + " - " ;
        }
        asTextResponse += getField(Appointment::Description).replace("\n", ". ") ;
        asTextResponse += end ;

    }

    return asTextResponse ;
}

QString& Appointment::shortText()
{
    QDateTime from = getDate(Appointment::From) ;
    QDateTime to = getDate(Appointment::To) ;

    shortTextResponse = from.toLocalTime().toString("dd-MMM") ;
    shortTextResponse += " at " ;
    shortTextResponse += to.toLocalTime().toString("hh:mm") ;
    return shortTextResponse ;
}

QString& Appointment::asHTML(QString name)
{
    return asText(name,
          "<span style=\" font-weight:600;\">",
          "</span><span style=\" color:#0000ff;\">",
          "</span>") ;
}

bool Appointment::isNotFor(QString& id)
{
    QString& apptfor = getField(Appointment::For) ;
    return (apptfor.compare(id)!=0) ;
}

bool Appointment::isInFuture()
{
    QDateTime now ;
    now = QDateTime::currentDateTimeUtc() ;
    return (getDate(Appointment::From) > now) ;
}

bool Appointment::isInRange(qint64 beforedays, qint64 afterdays)
{
    QDateTime now, before, after, to, from ;
    bool inrange ;

    now = QDateTime::currentDateTimeUtc() ;
    before = now.addDays(beforedays * -1) ;
    after = now.addDays(afterdays) ;
    to = getDate(Appointment::To) ;
    from = getDate(Appointment::From) ;
    inrange = (to > before && from < after) ;
    return inrange ;
}

bool Appointment::isCurrent()
{
    QDateTime from = getDate(Appointment::From) ;
    QDateTime to = getDate(Appointment::To) ;
    QDateTime now  = QDateTime::currentDateTimeUtc() ;
    return (now >= from && now <= to) ;
}


// Operators

Appointment& Appointment::operator=(const Appointment &rhs)
{
    if (!isnull) {
        if (this == &rhs) return *this;
        this->isdirty = rhs.isdirty ;
        this->isempty = rhs.isempty ;
        this->isnull = rhs.isnull ;
        this->shortTextResponse = rhs.shortTextResponse ;
        this->asHTMLResponse = rhs.asHTMLResponse ;
        this->asTextResponse = rhs.asTextResponse ;
        this->getDateResponse = rhs.getDateResponse ;
        for (int x=0; x<=(int)LASTRECORD; x++) {
            this->filedata[x] = rhs.filedata[x] ;
        }
    }
    return *this ;
}

// TODO: Check that this should copy GoogleLastSynced too
// depends how and where this function is actually used
Appointment& Appointment::copyGoogleAccountFieldsTo(Appointment& dest)
{
    for (int i=GOOGLEFIRSTRECORD; i<=GOOGLELASTRECORD; i++) {
        dest.setField((Appointment::AppointmentRecord)i, getField((Appointment::AppointmentRecord)i)) ;
    }
    return *this ;
}

Appointment& Appointment::copySyncedFieldsTo(Appointment& dest)
{
    for (int i=FIRSTSYNCEDRECORD; i<=LASTSYNCEDRECORD; i++) {
        dest.setField((Appointment::AppointmentRecord)i, getField((Appointment::AppointmentRecord)i)) ;
    }
    return *this ;
}


bool Appointment::matches(Appointment &with)
{
    bool match = true ;

    QString thisfield = getField(Appointment::GoogleAccount) ;
    QString thatfield = with.getField(Appointment::GoogleAccount) ;
    if (thisfield.compare(thatfield)!=0) match=false ;

    bool thisdeleted = isSet(Appointment::Deleted) ;
    bool thatdeleted = with.isSet(Appointment::Deleted) ;

    if (!(thisdeleted && thatdeleted)) for (int i=FIRSTSYNCEDRECORD; i<=LASTSYNCEDRECORD; i++) {
        QString thisfield = getField((Appointment::AppointmentRecord)i) ;
        QString thatfield = with.getField((Appointment::AppointmentRecord)i) ;
        if (thisfield.compare(thatfield)!=0) match=false ;
    }

    return match ;
}


bool Appointment::clashes(Appointment& with)
{
    //
    // Appointments clash if they are identical, or overlap
    //
    //         <***** with *****>
    // 1 ------------>                         =  This overlaps with the start of RHS
    // 2                   <----------------   = This overlaps with the end of RHS
    // 3           <--------->                 = This is within, or is identical to RHS
    // 4   <------------------------->         = RHS is within this
    //

    QDateTime dTo = getDate(Appointment::To) ;
    QDateTime dFrom = getDate(Appointment::From) ;
    QDateTime wTo = with.getDate(Appointment::To) ;
    QDateTime wFrom = with.getDate(Appointment::From) ;

    if (  (dTo > wFrom && dTo <= wTo) ||        // 1
          (dFrom > wFrom  && dFrom < wTo) ||    // 2
          (dFrom >= wFrom && dTo <= wTo) ||     // 3
          (dFrom < wFrom && dTo > wTo))  {      // 4
          return true ;
     } else {
        return false ;
    }
}

int Appointment::operator==(const Appointment& rhs) const
{
    // Supporting date search, but push deleted ones to the end
    if (rhs.isSet(Appointment::Deleted)) {
        return 1;
    }

    QDateTime f1 = getDate(Appointment::From) ;
    QDateTime f2 = rhs.getDate(Appointment::From) ;
    QDateTime t1 = getDate(Appointment::To) ;
    QDateTime t2 = rhs.getDate(Appointment::To) ;

    if (  f1==f2 && t1==t2){
        return 1 ;
    } else {
        return 0 ;
    }
}


int Appointment::operator<(const Appointment &rhs) const
{
    // Supporting date search, but push deleted ones to the end
    if (rhs.isSet(Appointment::Deleted)) {
        return 1;
    }

    QDateTime f1 = getDate(Appointment::From) ;
    QDateTime f2 = rhs.getDate(Appointment::From) ;
    QDateTime t1 = getDate(Appointment::To) ;
    QDateTime t2 = rhs.getDate(Appointment::To) ;

    if ( f1 < f2 ) {
        return 1 ;
    } else if (  (f1==f2) && (t1<t2) ) {
        return 1 ;
    } else {
        return 0 ;
    }
}

