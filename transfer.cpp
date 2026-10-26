#include "transfer.h"
#include "contactdatabase.h"
#include "calendar.h"

#include <algorithm>

// The contact fields that are exported, in the order they are written.  Flags are written as true or false.
static const Contact::ContactRecord contactFields[] = {
    Contact::ID, Contact::Surname, Contact::Names, Contact::Organisation, Contact::Address, Contact::Address2,
    Contact::Phone, Contact::PhoneTitle, Contact::Work, Contact::WorkTitle, Contact::Mobile, Contact::MobileTitle, Contact::Phone2, Contact::Phone2Title,
    Contact::Phone3, Contact::Phone3Title, Contact::Phone4, Contact::Phone4Title,
    Contact::Email, Contact::EmailTitle, Contact::Email2, Contact::Email2Title, Contact::Email3, Contact::Email3Title, Contact::Email4, Contact::Email4Title,
    Contact::Webaddress, Contact::Birthday, Contact::Comments,
    Contact::GroupBusiness, Contact::GroupClient, Contact::GroupFamily, Contact::GroupFriend, Contact::GroupOther,
    Contact::Hidden, Contact::EmailMe, Contact::TextMe, Contact::Created, Contact::Updated } ;

// The appointment fields that are exported, in the order they are written
static const Appointment::AppointmentRecord appointmentFields[] = {
    Appointment::ID, Appointment::ContactId, Appointment::For, Appointment::Summary, Appointment::Description,
    Appointment::Location, Appointment::From, Appointment::To, Appointment::Repeat, Appointment::RepeatInterval,
    Appointment::Flags, Appointment::Created } ;

// The name each field has in the file: the name it has in the record table
static QStringList contactColumnNames()
{
    Contact probe ;
    QStringList names ;
    for (Contact::ContactRecord f : contactFields) names << QString(probe.contactRecordName(f)) ;
    return names ;
}

static QStringList appointmentColumnNames()
{
    Appointment probe ;
    QStringList names ;
    for (Appointment::AppointmentRecord f : appointmentFields) names << QString(probe.appointmentrecordinfo[f].name) ;
    return names ;
}

// The lines of both histories, without repeats, newest first.  Lines with no readable date go last.
static QString mergeHistory(const QString &current, const QString &imported)
{
    QStringList all = current.split("\n", Qt::SkipEmptyParts) ;
    all << imported.split("\n", Qt::SkipEmptyParts) ;

    QStringList unique ;
    for (const QString &line : all) {
        if (!unique.contains(line)) unique << line ;
    }

    QList<QPair<QDateTime, QString> > entries ;
    for (const QString &line : unique) {
        entries << QPair<QDateTime, QString>(QDateTime::fromString(line.left(11), "dd-MMM-yyyy"), line) ;
    }
    std::stable_sort(entries.begin(), entries.end(),
        [](const QPair<QDateTime, QString> &x, const QPair<QDateTime, QString> &y) {
            if (x.first.isValid()!=y.first.isValid()) return x.first.isValid() ;
            return x.first > y.first ;
        }) ;

    QString text ;
    for (const QPair<QDateTime, QString> &entry : entries) {
        text += entry.second + "\n" ;
    }
    return text ;
}

QList<TransferRecord> contactTransferRecords(ContactDatabase &db)
{
    QStringList names = contactColumnNames() ;
    QList<TransferRecord> records ;

    for (int i=0; i<db.size(); i++) {
        Contact &c = db.getContact(i) ;
        if (c.isSet(Contact::Deleted)) continue ;

        TransferRecord record ;
        record.kind = "contact" ;
        for (int n=0; n<names.size(); n++) {
            Contact::ContactRecord f = contactFields[n] ;
            if (c.isContactOfType(f, Contact::mcFlag)) {
                record.add(names.at(n), c.isSet(f) ? "true" : "false") ;
            } else {
                record.add(names.at(n), c.getField(f)) ;
            }
        }
        record.add("history", c.getHistory().getHistory()) ;
        record.add("todo", c.getTodo().getText()) ;
        records << record ;
    }
    return records ;
}

QList<TransferRecord> appointmentTransferRecords(Calendar &cal)
{
    QStringList names = appointmentColumnNames() ;
    QList<TransferRecord> records ;

    for (int i=0; i<cal.size(); i++) {
        Appointment &a = cal.getAppointment(i) ;
        if (a.isSet(Appointment::Deleted) || a.isSet(Appointment::InternetOwned) || a.isBirthday()) continue ;

        TransferRecord record ;
        record.kind = "appointment" ;
        for (int n=0; n<names.size(); n++) {
            record.add(names.at(n), a.getField(appointmentFields[n])) ;
        }
        records << record ;
    }
    return records ;
}

bool contactExists(ContactDatabase &db, const TransferRecord &rec)
{
    QString id = rec.value("id").trimmed() ;
    return !id.isEmpty() && !db.getContactBy(Contact::ID, id).isNull() ;
}

bool appointmentExists(Calendar &cal, const TransferRecord &rec)
{
    QString id = rec.value("id").trimmed() ;
    return !id.isEmpty() && !cal.getAppointmentBy(Appointment::ID, id).isNull() ;
}

void importContact(ContactDatabase &db, const TransferRecord &rec, bool overwrite)
{
    bool exists = contactExists(db, rec) ;
    if (exists && !overwrite) return ;

    QString id = rec.value("id").trimmed() ;
    Contact &c = exists ? db.getContactBy(Contact::ID, id) : db.addContact() ;
    if (!exists) {
        if (id.isEmpty()) c.createNew() ;
        else c.setField(Contact::ID, id) ;
    }

    // The fields in the file.  An existing record keeps its creation date, and Updated is set below.
    QStringList names = contactColumnNames() ;
    for (int n=0; n<names.size(); n++) {
        Contact::ContactRecord f = contactFields[n] ;
        if (f==Contact::ID || f==Contact::Updated) continue ;
        if (f==Contact::Created && exists) continue ;
        if (!rec.has(names.at(n))) continue ;
        if (c.isContactOfType(f, Contact::mcFlag)) {
            c.setFlag(f, rec.value(names.at(n))=="true") ;
        } else {
            c.setField(f, rec.value(names.at(n))) ;
        }
    }

    c.setFlag(Contact::Deleted, false) ;
    if (rec.has("todo")) {
        QString todo = rec.value("todo") ;
        c.getTodo().setText(todo) ;
    }
    if (rec.has("history")) c.getHistory().updateHistory(mergeHistory(c.getHistory().getHistory(), rec.value("history"))) ;
    if (c.getField(Contact::Created).isEmpty()) c.setDate(Contact::Created, QDateTime::currentDateTime()) ;

    // Updated is set now, so that the next Google Sync sends the change
    c.setDate(Contact::Updated, QDateTime::currentDateTimeUtc()) ;
    c.markAsDirty() ;
    c.getHistory().addEntry(exists ? "Overwritten by an import" : "Added by an import") ;
}

void importAppointment(Calendar &cal, const TransferRecord &rec, bool overwrite)
{
    bool exists = appointmentExists(cal, rec) ;
    if (exists && !overwrite) return ;

    QString id = rec.value("id").trimmed() ;

    // A new appointment is built here and added at the end.  An existing one is updated in place, so that
    // its Google link is kept.
    Appointment fresh ;
    if (!exists) {
        fresh.createNew() ;
        if (!id.isEmpty()) fresh.setField(Appointment::ID, id) ;
    }
    Appointment &a = exists ? cal.getAppointmentBy(Appointment::ID, id) : fresh ;

    QStringList names = appointmentColumnNames() ;
    for (int n=0; n<names.size(); n++) {
        Appointment::AppointmentRecord f = appointmentFields[n] ;
        if (f==Appointment::ID) continue ;
        if (f==Appointment::Created && exists) continue ;
        if (!rec.has(names.at(n))) continue ;
        a.setField(f, rec.value(names.at(n))) ;
    }

    a.setFlag(Appointment::Deleted, false) ;
    a.setDate(Appointment::Updated, QDateTime::currentDateTimeUtc()) ;
    a.markAsDirty() ;

    if (!exists) cal.addAppointment(fresh) ;
}

QString transferDescription(const TransferRecord &rec)
{
    if (rec.kind==QLatin1String("contact")) {
        QString names = rec.value("names").trimmed() ;
        QString surname = rec.value("surname").trimmed() ;
        QString name ;
        if (surname.isEmpty()) name = names ;
        else if (names.isEmpty()) name = surname ;
        else name = surname + ", " + names ;
        if (name.isEmpty()) name = rec.value("email").trimmed() ;
        if (name.isEmpty()) name = "unnamed contact" ;
        return "Contact '" + name + "'" ;
    }

    QString summary = rec.value("summary").trimmed() ;
    if (summary.isEmpty()) summary = "(no summary)" ;
    return "Appointment '" + summary + "' starting " + rec.value("from").trimmed() ;
}
