#include "contact.h"
#include "configuration.h"
#include <QUuid>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QTextDecoder>
#include <QDebug>
#include "history.h"
#include "../Lib/supportfunctions.h"

Contact::Contact()
{
    isnull = false ;
    createNew() ;
}

Contact::~Contact()
{
}

Todo& Contact::getTodo()
{
    return todo ;
}

History& Contact::getHistory()
{
    return history ;
}

bool Contact::setNull()
{
    filedata[ID] = "00000000-0000-0000-0000-000000000000" ;
    isnull=true ;
    return true ;
}

bool Contact::isNull()
{
    return isnull ;
}

bool Contact::isEmpty()
{
    return isempty && history.isEmpty() && todo.isEmpty() ;
}

bool Contact::isDirty()
{
    return isdirty || history.isdirty() || todo.isdirty() ;
}

bool Contact::changedSinceSync(QDateTime &syncdate)
{
    QDateTime updated = getDate(Contact::Updated) ;
    return (updated > syncdate) ;
}


void Contact::markAsSaved()
{
    isdirty=false ;
}

void Contact::markAsDirty()
{
    isdirty=true ;
}

bool Contact::createNew()
{
    QUuid qid ;

    for (int i=FIRSTRECORD; i<=LASTRECORD; i++) {
        filedata[i].clear() ;
//        setField((enum ContactRecord)i, QString("")) ;
    }

    setFlag(Contact::TextMe, false) ;
    setFlag(Contact::EmailMe, false) ;
    setFlag(Contact::Deleted, false) ;
    setFlag(Contact::Hidden, false) ;
    setFlag(Contact::GoogleDeleted, false) ;
    setFlag(Contact::GoogleUploaded, false) ;
    setField(Contact::Group, gUNKNOWN) ;

    qid = QUuid::createUuid() ;
    if (qid.isNull()) {
        // Error
        setNull() ;
    } else {
        QString qids ;
        isnull=false ;
        qids=qid.toString();
        qids.replace("{","") ;
        qids.replace("}","") ;
        setField(Contact::ID,  qids) ;
        todo.createNew(qids) ;
    }

    setField(Contact::Created, getField(Contact::Updated)) ;

    getOverviewDirty = true ;
    getOverviewHtmlDirty = true ;
    getOverviewResponse = "" ;
    getOverviewResponseHtml = "" ;

    formattedNameResponse = "" ;
    emptystring = "" ;
    googleXml = "";

    isempty = true ;
    isdirty = false ;
    return true ;
}


char *Contact::getContactRecordName(enum ContactRecord field) {

    switch (field) {
        case Group: return (char *)"group" ; break ;
        case Surname: return  (char *)"surname" ; break ;
        case Names: return  (char *)"names" ; break ;
        case Organisation: return  (char *)"organisation" ; break ;
        case Address: return  (char *)"address" ; break ;
        case Address2: return  (char *)"address2" ; break ;
        case Phone: return  (char *)"phone" ; break ;
        case Phone2: return  (char *)"otherphone" ; break ;
        case Work: return  (char *)"work" ; break ;
        case Mobile: return  (char *)"mobile" ; break ;
        case Voip: return  (char *)"voip" ; break ;
        case Email: return  (char *)"email" ; break ;
        case Email2: return  (char *)"email2"; break ;
        case Phone2Title: return  (char *)"otherphonetitle" ; break ;
        case Webaddress: return (char *)"webaddress"; break ;
        case Birthday: return  (char *)"birthday" ; break ;
        case Comments: return  (char *)"comments" ; break ;
        case EmailMe: return  (char *)"emailme" ; break ;
        case TextMe: return  (char *)"textme" ; break ;
        case ID: return (char *)"id" ; break ;
        case Updated: return (char *)"updated" ; break ;
        case Created: return (char *)"created" ; break ;
        case Deleted: return  (char *)"deleted" ; break ;
        case Hidden: return  (char *)"hidden" ; break ;
        case GoogleDeleted: return  (char *)"googledeleted" ; break ;
        case GoogleUploaded: return (char *)"googleuploaded" ; break ;
        case GoogleAccount: return (char *)"googleaccount" ; break ;
        case GoogleRecordId: return (char *)"googlerecordid" ; break ;
        case GoogleEtag: return (char *)"googleetag" ; break ;
        case GoogleSequence: return (char *)"googlesequence"; break ;
        case GoogleCreated: return (char *)"googlecreated" ; break ;
//        case GoogleStatus: return (char *)"googlestatus" ; break ;
        default: return  (char *)"END" ; break ;
    }
}

bool Contact::isForAccount(QString googleaccount)
{
    return (getField(Contact::GoogleAccount).compare(googleaccount)==0) ;
}

// TODO: Make this load a "safe load", i.e. check for failed saves
bool Contact::load(QString path, QString idname, Encryption *enc)
{
    if (isnull || !enc) {
        // ERROR
        return false ;
    }

    QString Line ;
    QStringList ParsedLine;
    QRegExp sep("(\\=)");

    createNew() ;

    isdirty = false ;

    // Attempt to load encrypted contact .zcontact and fall back to legacy .contact
    QByteArray data ;
    if (!enc->load(path + idname + ".zcontact", data)) {
        QFile file(path + idname + ".contact");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        data = file.readAll() ;
        file.close() ;
    }
    if (data.isEmpty()) return false ;

    QTextStream in(&data);
    in.setCodec("UTF-8") ;

    // Isempty is updated by setfield
    isempty = true ;

    while (!in.atEnd()) {
        Line = in.readLine();
        ParsedLine =  Line.split(sep);

        // TODO: Handle cases where there is an "=" in the middle of the string

        if (ParsedLine.count()==2) {
            for (int i=FIRSTRECORD; i<=LASTRECORD; i++) {
                QString label = ParsedLine.at(0) ;
                QString entrytext = ParsedLine.at(1) ;
                // TODO: de-escape entrytext
                if (label.compare(getContactRecordName((enum ContactRecord)i))==0) {
                    setField((enum ContactRecord)i, entrytext.replace("\\n","\n").replace("\r","")) ;
                }
            }
        }
    }

    todo.load(path, idname, enc) ;
    history.load(path, idname, enc) ;

    isdirty = false ;
    return true ;
}

int Contact::find(QString text)
{
    text = text.toLower() ;
    deAccent(text) ;
    text.replace(" ", "") ;
    QRegExp re2(".*(" + text + ").*") ;

    // Search in the overview
    QString overview = getOverview(contactAsText).toLower() ;
    deAccent(overview) ;
    overview.replace(" ", "") ;    
    overview.replace(",", "") ;
    overview.replace(".", "") ;
    if (re2.exactMatch(overview)) {
        qDebug() << "Contact::find(" << text << ") Match Overview {" << getField(Contact::ID) << "}\n" ;
        return 1;
    }

    // Search in a firstname middlename surname
    QString name = firstname + surname ;
    name.replace(" ", "") ;
    if (re2.exactMatch(name)) {
        qDebug() << "Contact::find(" << text << ") Match Firstname Midddlename Surname {" << getField(Contact::ID) << "}\n" ;
        return 1;
    }

    // Search in a firstname surname
    QStringList names = firstname.split(" ") ;
    if (names.size()>0) {
        name = names.at(0) + surname ;
        if (re2.exactMatch(name)) {
            if (re2.exactMatch(name)) {
                qDebug() << "Contact::find(" << text << ") Match Firstname Surname {" << getField(Contact::ID) << "}\n" ;
                return 1;
            }
        }
    }

    // Search in a surname firstname
    name = surname + firstname ;
    name.replace(" ", "") ;
    if (re2.exactMatch(name)) {
        if (re2.exactMatch(name)) {
            qDebug() << "Contact::find(" << text << ") Match Surname Firstname {" << getField(Contact::ID) << "}\n" ;
            return 1;
        }
    }

    // No Match Found
    return -1 ;
}

// TODO: Make this save a "safe save", i.e. save, check, rename
bool Contact::save(QString path, Encryption *enc)
{

  if (isnull) {
      // ERROR
      return false ;
  }

  if (!isempty && todo.isdirty()) todo.save(path + filedata[ID], enc) ;
  if (!isempty && history.isdirty()) history.save(path + filedata[ID]) ;

  if (isdirty) {

      QByteArray data ;
      QTextStream out(&data, QIODevice::WriteOnly);
      out.setCodec("UTF-8") ;
      out << "[contact]\n" ;
      for (int entry=FIRSTRECORD; entry<=LASTRECORD; entry++) {
          QString entrytext = filedata[entry].trimmed().replace("\n","\\n");
          out << getContactRecordName((enum ContactRecord)entry) << "=" << entrytext << "\n" ;
      }

      out.flush();
      if (!enc->save(path + filedata[ID] + ".zcontact", data)) {
          return false ;
      }

      isdirty = false ;
  }
  return true ;
}


bool Contact::isSet(enum Contact::ContactRecord field)
{
    return getField(field).compare("true")==0 ;
}

bool Contact::isTristate(enum Contact::ContactRecord field)
{
    return getField(field).isEmpty() ;
}

void Contact::setFlag(enum Contact::ContactRecord field, bool flag)
 {
     if (flag) {
         setField(field, "true") ;
     } else {
         setField(field, "false") ;
     }
}

QString& Contact::getField(enum Contact::ContactRecord field)
{
    static QString nullstring="" ;
    static QString otherstring="Other" ;
    static QString groupstring="Unknown" ;

    if (!isnull && field==Group && filedata[Group].isEmpty())
        setField(Group, groupstring) ;

    if (!isnull && field==Phone2Title && filedata[Phone2Title].isEmpty())
        setField(Phone2Title, otherstring) ;

    if ((field<FIRSTRECORD || field>LASTRECORD)) return nullstring ;
    else return filedata[field] ;
}


void Contact::setField(enum Contact::ContactRecord field, QString data)
{
    if (isnull) {
        // ERROR, attempting to set NULL Contact
        return ;
    }

    // Override flags
    if (field == Contact::EmailMe || field == Contact::TextMe || field == Contact::Hidden ||
            field == Contact::Deleted || field == Contact::GoogleDeleted || field == Contact::GoogleUploaded) {
        if (data=="yes") data = "true" ;
        if (data=="no") data = "false" ;
    }

    QString dat ;
    dat = data.replace("\r"," ") ;
    dat = dat.trimmed() ;

    if (field>=STARTPHONE && field<=ENDPHONE)
        dat = parsePhoneNumber(dat) ;

    if (field==Birthday)
        dat = parseDate(dat) ;

    if (field==Phone2Title)
        dat = dat.left(1).toUpper() + dat.mid(1).toLower() ;

    if (field==GoogleUploaded)
        dat = dat ;

    if (field>=FIRSTRECORD && field<=LASTRECORD && dat.compare(filedata[field])!=0) {
        // Store data
        filedata[field]=dat ;
        isdirty = true ;
        getOverviewDirty = true ;
        getOverviewHtmlDirty = true ;
        if (field>=FIRSTSYNCEDDATA && field<=LASTSYNCEDDATA && !filedata[field].isEmpty()) {
            // Update empty status
            isempty=false ;
        }
        if (field>=FIRSTSYNCEDRECORD && field<=LASTSYNCEDRECORD) {
            // Update timestamp
            QString now = nowToIsoString() ;
            filedata[Contact::Updated] = now ;
        }
    }

    if (field==Surname || field==Names || field==Organisation) {
        surname = filedata[Surname].toLower() ;
        deAccent(surname) ;
        firstname = filedata[Names].toLower() ;
        deAccent(firstname) ;
        organisation = filedata[Organisation].toLower() ;
        deAccent(organisation) ;
        sortstring = surname + firstname + organisation ;
        sortstring.replace(" ", "") ;
        sortstring.replace(",", "") ;
        sortstring.replace(".", "") ;
    }

    isnull = false ;
}

void Contact::setDate(enum ContactRecord field, QDateTime data)
{
    QString datestr = dateTimeToIsoString(data) ;
    setField(field, datestr) ;
}

QDateTime& Contact::getDate(enum ContactRecord field)
{
    static QDateTime dt ;
    QString data = getField(field) ;
    dt = isoStringToDateTime(data) ;
    return dt ;
}

QString& Contact::parseDate(QString src)
{
    static QString result ;
    result = src ;
    return result ;
}

QString& Contact::parsePhoneNumber(QString src)
{
    static QString result ;

    // Leave empty and short phone number records alone
    if (src.isEmpty() || src.length()<=4) {
        result = src ;
        return result ;
    }

    // Remove all Spaces
    result = src.replace(" ","") ;

    // Replace leading 00 with +
    // [...].replace(QRegExp("((?:https?|ftp)://\\S+)"), "<a href=\"\\1\">\\1</a>")
    result = result.replace(QRegExp("^00(.*)"), "+\\1") ;

    // If the number starts with +, assume it's got all the right digits
    if (result.left(1).compare("+")!=0) {

        // Add a Local Dialling Code if Missing
        if (result.left(1).compare("0")!=0 && !gConf->getLocalDiallingCode().isEmpty()) {
            result = gConf->getLocalDiallingCode() + result ;
            result = result.replace(QRegExp("^00(.*)"), "+\\1") ;
        }

        // Add a Country Code if Missing
        if (result.left(1).compare("+")!=0 && !gConf->getCountryDiallingCode().isEmpty()) {
            result = result.replace(QRegExp("^0(.*)"), gConf->getCountryDiallingCode() + "\\1") ;
            result = result.replace(QRegExp("^00(.*)"), "+\\1") ;
        }

    }

    // 01	Landlines (geographic)	Various – see below
    // 02	Landlines (geographic)	02x xxxx xxxx
    // 03	Landlines (non-geographic)	03xx xxx xxxx
    // 04	Not used	-
    // 05	Corporate numbering	05xxx xxxxxx
    // 06	Not used	-
    // 07	Mobiles, pagers and personal	07xxx xxxxxx
    // 08	Special rate	08xx xxx xxxx
    // 09	Premium rate	09xx xxx xxxx

    // Format spaces for the Country

    QRegExp other("\\+(\\d\\d)(\\d*)$") ;
    QRegExp france("\\+33(\\d*)(\\d\\d)(\\d\\d)(\\d\\d)$") ;
    QRegExp uk2("\\+44(2\\d)(\\d\\d\\d\\d)(\\d\\d\\d\\d)$") ;
    QRegExp uk389("\\+44([389]\\d\\d)(\\d\\d\\d)(\\d\\d\\d\\d)$") ;
    QRegExp uk("\\+44(\\d*)(\\d\\d\\d\\d\\d\\d)$") ;

    if (france.indexIn(result)>=0) {
        result = "+33 " + france.cap(1) + " " + france.cap(2) + " " + france.cap(3) + " " + france.cap(4) ;
    } else if (uk2.indexIn(result)>=0) {
        result = "+44 " + uk2.cap(1) + " " + uk2.cap(2) + " " + uk2.cap(3);
    } else if (uk389.indexIn(result)>=0) {
        result = "+44 " + uk389.cap(1) + " " + uk389.cap(2) + " " + uk389.cap(3);
    } else if (uk.indexIn(result)>=0) {
        result = "+44 " + uk.cap(1) + " " + uk.cap(2) ;
    } else if (other.indexIn(result)>=0) {
        result = "+" + other.cap(1) + " " + other.cap(2) ;
    }

    return result ;
}

QString& Contact::getGoogleRecordId()
{
    return filedata[Contact::GoogleRecordId] ;
}

QString& Contact::getFormattedName(bool includeorganisation, bool surnamefirst)
{
    if (filedata[Names] == "" && filedata[Surname] == "" && filedata[Organisation] != "") {
        formattedNameResponse = filedata[Organisation] ;
    } else if (filedata[Names] == "" && filedata[Surname] == "") {
        formattedNameResponse = filedata[Email] ;
        if (formattedNameResponse.isEmpty()) formattedNameResponse = filedata[Email2] ;
        if (formattedNameResponse.isEmpty()) formattedNameResponse = "Unnamed Contact" ;
    } else if (filedata[Surname] == "" && (filedata[Organisation] == "" || !includeorganisation)) {
        formattedNameResponse = filedata[Names] ;
    } else if (filedata[Surname] == "" && (filedata[Organisation] != "" && includeorganisation)) {
        formattedNameResponse = filedata[Names] + " (" + filedata[Organisation] + ")" ;
    } else {
        if (surnamefirst) formattedNameResponse = filedata[Surname] + ", " + filedata[Names] ;
        else formattedNameResponse = filedata[Names] + " " + filedata[Surname] ;
        if (includeorganisation && !filedata[Organisation].isEmpty())
            formattedNameResponse = formattedNameResponse + " (" + filedata[Organisation] + ")" ;
    }
    return formattedNameResponse ;
}

QString& Contact::asText()
{
    return getFormattedName(false, false) ;
}

QString& Contact::getOverview(enum ContactOverviewType overviewtype)
{

    if (overviewtype == contactAsText && !getOverviewDirty) {
        return getOverviewResponse ;
    }
    if (overviewtype == contactAsHTML && !getOverviewHtmlDirty) {
        return getOverviewResponseHtml ;
    }

    QString titlestart, titleend ;
    QString lineend ;

    QString location ;
    if (getField(Group).compare(gBUSINESS)==0) {
        location = "Work" ;
    } else {
        location = "Home" ;
    }

    // TODO: Use Stylesheets
    if (overviewtype == contactAsHTML) {
        titlestart = "<b>" ;
        titleend = "</b>" ;
        lineend = "<br/>\n" ;
    } else /* asText && asVCARD */ {
        titlestart = "" ;
        titleend = "" ;
        lineend = "\n" ;
    }

    // TODO: This is in the header file, but generates errors if not declared here
    QString response ;

    if (isSet(Contact::Hidden)) response += titlestart + "Hidden Status: " + titleend + "Hidden" + lineend ;
    if (!getField(Phone).isEmpty()) response += titlestart + "Phone: " + titleend + getField(Phone) + lineend ;
    if (!getField(Mobile).isEmpty()) response += titlestart + "Mobile:  " + titleend + getField(Mobile) + lineend ;
    if (!getField(Work).isEmpty()) response += titlestart + "Work Phone:  " + titleend + getField(Work) + lineend ;
    QString phone2Title = getField(Phone2Title) + " Phone" ;
    if (!getField(Phone2).isEmpty()) response += titlestart + phone2Title + ": " + titleend + getField(Phone2) + lineend ;
    if (!getField(Voip).isEmpty()) response += titlestart + "Voip / Skype:  " + titleend + getField(Voip) + lineend ;
    if (!getField(Email).isEmpty()) response += titlestart + location + " Email:  " + titleend + getField(Email) + lineend ;
    if (!getField(Email2).isEmpty()) response += titlestart + "Second Email:  " + titleend + getField(Email2) + lineend ;
    if (!getField(Webaddress).isEmpty()) response += titlestart + "Web Site:  " + titleend + getField(Webaddress) + lineend ;
    if (!getField(Organisation).isEmpty()) response += titlestart + "Organisation: " + titleend + getField(Organisation) + lineend ;

    if (!getField(Address).isEmpty()) {
        QString a = getField(Address) ;
        a.replace(",", "") ;
        a.replace("\n", ", ") ;
        response += titlestart + location + " Address:  " + titleend + a + lineend ;
    }
    if (!getField(Address2).isEmpty()) {
        QString a = getField(Address2) ;
        a.replace(",", "") ;
        a.replace("\n", ", ") ;
        response += titlestart + "Second Address:  " + titleend + a + lineend ;
    }
    if (!getField(Birthday).isEmpty()) {
        QString a = getField(Birthday) ;
        response += titlestart + "Birthday: " + titleend + a.replace("\n",lineend) + lineend ;
    }
    if (!getField(Group).isEmpty()) response += titlestart + "Type: " + titleend + getField(Group).toCaseFolded() + lineend ;
    if (isSet(EmailMe)) response += titlestart + "Alerts by Email: " + titleend + "OK" + lineend ;
    if (isSet(TextMe)) response += titlestart + "Alerts by Text: " + titleend + "OK" + lineend ;

    if (!getField(Comments).isEmpty()) {
        QString a = getField(Comments) ;
        response += titlestart + "Comments: " + titleend + lineend +  a.replace("\n",lineend) + lineend ;
    }
    if (overviewtype == contactAsHTML) {
        getOverviewHtmlDirty = false ;
        getOverviewResponseHtml = response ;
        return getOverviewResponseHtml ;
    } else {
        getOverviewDirty = false ;
        getOverviewResponse = response ;
        return getOverviewResponse ;
    }

}


// Operators

Contact& Contact::operator=(const Contact &rhs)
{
    if (isnull) {
        // ERROR
        return *this ;
    }
    if (this == &rhs) return *this;

    this->isdirty = rhs.isdirty;
    this->isempty = rhs.isempty ;
    this->todo = rhs.todo ;
    this->history = rhs.history ;
    this->sortstring = rhs.sortstring ;
    this->surname = rhs.surname ;
    this->firstname = rhs.firstname ;
    this->getOverviewDirty = true ;
    this->getOverviewHtmlDirty = true ;

    for (int x=FIRSTRECORD; x<=(int)LASTRECORD; x++) {
        this->filedata[x]=rhs.filedata[x] ;
    }
    return *this ;
}

Contact& Contact::copyGoogleAccountFieldsTo(Contact& dest)
{
    for (int i=Contact::GOOGLEFIRSTRECORD; i<=Contact::GOOGLELASTRECORD; i++) {
        dest.setField((Contact::ContactRecord)i, getField((Contact::ContactRecord)i)) ;
    }
    return *this ;
}

Contact& Contact::copySyncedFieldsTo(Contact& dest)
{
    for (int i=Contact::FIRSTSYNCEDRECORD; i<=Contact::LASTSYNCEDRECORD; i++) {
        dest.setField((Contact::ContactRecord)i, getField((Contact::ContactRecord)i)) ;
    }
    return *this ;
}


Contact& Contact::getThis()
{
    return *this ;
}

/*
//
// Compare two contacts within a given scope
//
bool Contact::compare(Contact &other, enum CompareScope scope)
{
    bool match=false ;
    switch (scope) {
    case cName:
        match = (getField(Surname).compare(other.getField(Surname))==0 &&
                 getField(Names).compare(other.getField(Names))==0) ;
        break ;
    case cFull:
        match=true ;
        for (int i=FIRSTRECORD; i<LASTRECORD && match; i++) {
            if (getField((enum ContactRecord)i).compare(other.getField((enum ContactRecord)i))!=0) {
                match=false ;
            }
        }
        break ;
    case cDetails:
        match=true ;
        for (int i=(ContactRecord)FIRSTSYNCEDRECORD; i<LASTRECORD; i++) {
                if (getField((enum ContactRecord)i).compare(other.getField((enum ContactRecord)i))!=0) {
                    match=false ;
                }
        }
        break ;
    case cID:
        match = (getField(ID).compare(other.getField(ID))==0) ;
        break ;
    case cGoogleID:
        // TODO: Should really check the Google Account too
        match = (getField(GoogleAccount).compare(other.getField(GoogleAccount))==0) ;
        match &= (getField(GoogleRecordId).compare(other.getField(GoogleRecordId))==0) ;
        break ;
    }
    return match ;
}
*/

bool Contact::matches(Contact &with)
{
    bool match = true ;

    QString thisfield = getField(Contact::GoogleAccount) ;
    QString thatfield = with.getField(Contact::GoogleAccount) ;
    if (thisfield.compare(thatfield)!=0) match=false ;

    bool thisdeleted = isSet(Contact::Deleted) ;
    bool thatdeleted = with.isSet(Contact::Deleted) ;

    if (match && !(thisdeleted && thatdeleted)) {

        for (int i=FIRSTSYNCEDRECORD; i<=LASTSYNCEDRECORD; i++) {

          QString thisfield = getField((Contact::ContactRecord)i) ;
          QString thatfield = with.getField((Contact::ContactRecord)i) ;
          if (thisfield.compare(thatfield)!=0) match=false ;

      }

    }


    return match ;
}


//
// Operators used for the sort functions
//
int Contact::operator==(const Contact &rhs) const
{
    if (rhs.filedata[Contact::Hidden].compare("true")==0 ||
            rhs.filedata[Contact::Deleted].compare("true")==0) return false;
    return (sortstring.compare(rhs.sortstring)==0) ;
}

int Contact::operator<(const Contact &rhs) const
{
    if (rhs.filedata[Contact::Hidden].compare("true")==0 ||
            rhs.filedata[Contact::Deleted].compare("true")==0) return false;
    return (sortstring.compare(rhs.sortstring)<0) ;
}

