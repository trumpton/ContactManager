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

// Debug Trap
//#define TRAPID "d8d7df66-36b2-44d2-91c2-6f6835bd5e78"

Contact::Contact(bool isNull)
{
    // Check Params
    contactrecordinfook=true ;
    for (int i=0; i<NumberOfRecords; i++) {
        if (contactrecordinfo[i].recordtype!=(Contact::ContactRecord)i) {
            contactrecordinfook=false ;
        }
    }

    isnull = false ;
    isnew = false ;
    createNew() ;
    if (isNull) setNull() ;
}

Contact::~Contact()
{
}

Contact::Contact(const Contact &rhs)
{
    isnull = false ;
    *this = rhs ;
}

// COPY: Copies all fields
Contact& Contact::operator=(const Contact &rhs)
{
    if (isnull) {
        qFatal("ERROR: Copying into NULL contact entry not allowed") ;
        return *this ;
    }
    if (this == &rhs) return *this;

    this->contactrecordinfook = rhs.contactrecordinfook ;

    this->emptystring="" ;
    this->googleXml="" ;
    this->mergedidlist.clear() ;
    this->sortstring="" ;
    this->getOverviewResponse="" ;
    this->getOverviewResponseHtml="" ;

    this->isnew = rhs.isnew ;
    this->isdirty = rhs.isdirty;
    this->isempty = rhs.isempty ;
    this->todo = rhs.todo ;
    this->history = rhs.history ;
    this->sortstring = rhs.sortstring ;
    this->surname = rhs.surname ;
    this->firstname = rhs.firstname ;
    this->organisation = rhs.organisation ;
    this->formattedNameResponse = rhs.formattedNameResponse ;

    this->googleSaveDateTimeResponse = rhs.googleSaveDateTimeResponse ;
    this->contactSaveDateTimeResponse = rhs.contactSaveDateTimeResponse ;

    this->getOverviewDirty = true ;
    this->getOverviewHtmlDirty = true ;

    for (int x=0; x<Contact::NumberOfRecords; x++) {
        this->filedata[x]=rhs.filedata[x] ;
    }
    return *this ;
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
    isnew=false ;
}

void Contact::markAsDirty()
{
    isdirty=true ;
}

bool Contact::createNew()
{
    QUuid qid ;

    // Clear all fields
    for (int i=0; i<Contact::NumberOfRecords; i++) {
        if (isContactOfType((Contact::ContactRecord)i, Contact::mcFlag)) { setFlag((Contact::ContactRecord)i, false) ; }
        else { setField((Contact::ContactRecord)i, QString("")) ; }
    }

    // Set Contact ID
    qid = QUuid::createUuid() ;
    if (qid.isNull()) {
        // Error
        dbg("ERROR: error creating UUID") ;
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
    isnew = true ;
    return true ;
}

// Merge History and ToDo
void Contact::mergeInto(Contact& other)
{
    dbg(QString("{%1} mergeinto(%2)").arg(getField(Contact::ID).arg(other.getField(Contact::ID)))) ;

    QString otherhistory = other.getHistory().getHistory() ;
    QString newhistory = getHistory().getHistory() + otherhistory;
    other.getHistory().updateHistory(newhistory) ;

    QString othertodo = other.getTodo().getText() ;
    QString newtodo = getTodo().getText() + othertodo ;
    other.getTodo().setText(newtodo) ;
}


//bool Contact::isRecordFlag(enum ContactRecord field) { return contactrecordinfo[(int)field].isflag ; }
char *Contact::contactRecordName(enum ContactRecord field) { return (char *)contactrecordinfo[(int)field].name ; }
//bool Contact::isRecordSynced(enum ContactRecord field) { return contactrecordinfo[(int)field].issynced ; }


// TODO: Make this load a "safe load", i.e. check for failed saves
// TODO: Handle cases where there is an "=" in the middle of the string
bool Contact::load(QString path, QString idname, Encryption *enc)
{
    if (isnull || !enc) {
        // ERROR
        dbg("ERROR: load() - Contact record is null or encryption not configured") ;
        return false ;
    }

    dbg(QString("load(%1,%2,enc)").arg(path).arg(idname)) ;

    QString Line ;
    QStringList ParsedLine;
    QRegExp sep("(\\=)");
    bool oldformat = false ;

    createNew() ;

    isdirty = false ;

    // Attempt to load encrypted contact .zcontact and fall back to legacy .contact
    QByteArray data ;

    if (!enc->load(path + idname + ".zcontact", data)) {

        dbg(QString("Encrypted load failed. Trying non-encrypted.")) ;
        QFile file(path + idname + ".contact");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        data = file.readAll() ;
        file.close() ;
    }
    if (data.isEmpty()) {
        dbg("No data loaded") ;
        return false ;
    }

    QTextStream in(&data);
    in.setCodec("UTF-8") ;

    // Isempty is updated by setfield
    isempty = true ;

    while (!in.atEnd()) {
        Line = in.readLine();
        ParsedLine =  Line.split(sep);

        if (ParsedLine.count()==2) {

            QString label = ParsedLine.at(0) ;
            QString entrytext = ParsedLine.at(1) ;

            // Override old ini file labels
            if (label.compare("googleetag")==0) { label = "" ; }
            if (label.compare("otherphone")==0) { label = "phone2" ; oldformat=true ; }
            if (label.compare("otherphonetitle")==0) { label = "phone2title" ; oldformat=true ; }
            if (label.compare("group")==0) {
                if (entrytext.compare("Business")==0) { label = "groupbusiness" ; }
                if (entrytext.compare("Client")==0) { label = "groupclient" ; }
                if (entrytext.compare("Family")==0) { label = "groupfamily" ; }
                if (entrytext.compare("Friends")==0) { label = "groupfriend" ; }
                if (entrytext.compare("Unknown")==0) { label = "groupother" ; }
                entrytext = "true" ;
                oldformat = true ;
            }

            // Fix date: --mm-dd to -mm-dd
            if (label.compare("birthday")==0) {
                QStringList dmy = entrytext.split('-') ;
                if (dmy.size()==4) {
                    entrytext = dmy.at(1) + QString("-") + dmy.at(2) + QString("-") + dmy.at(3) ;
                }
            }

            for (int i=0; i<Contact::NumberOfRecords; i++) {

                if (label.compare(contactRecordName((enum ContactRecord)i))==0) {
                    setField((enum ContactRecord)i, entrytext.replace("\\n","\n").replace("\r","")) ;
                }

            }
        }
    }

    // Remove any rogue phone titles, should there be no phone number
    if (getField(Contact::Phone2).isEmpty()) { setField(Contact::Phone2Title, "") ; }
    if (getField(Contact::Phone3).isEmpty()) { setField(Contact::Phone3Title, "") ; }
    if (getField(Contact::Phone4).isEmpty()) { setField(Contact::Phone4Title, "") ; }

    sortPhoneNumbers() ;
    todo.load(path, idname, enc) ;
    history.load(path, idname, enc) ;

    isnew = false ;
    isdirty = false ;
    if (oldformat) isdirty = true ;

    return true ;
}


int Contact::find(QString text)
{
    dbg(QString("{%1} - find(%2)").arg(getField(Contact::ID).arg(text))) ;

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
        dbg(QString("Match Overview")) ;
        return 1;
    }

    // Search in a firstname middlename surname
    QString name = firstname + surname ;
    name.replace(" ", "") ;
    if (re2.exactMatch(name)) {
        dbg(QString("Match Firstname Middlename Surname")) ;
        return 1;
    }

    // Search in a firstname surname
    QStringList names = firstname.split(" ") ;
    if (names.size()>0) {
        name = names.at(0) + surname ;
        if (re2.exactMatch(name)) {
            if (re2.exactMatch(name)) {
                dbg(QString("Match Firstname Surname")) ;
                return 1;
            }
        }
    }

    // Search in a surname firstname
    name = surname + firstname ;
    name.replace(" ", "") ;
    if (re2.exactMatch(name)) {
        if (re2.exactMatch(name)) {
            dbg(QString("Match Surname Firstname")) ;
            return 1;
        }
    }

    // No Match Found
    return -1 ;
}

// TODO: Make this save a "safe save", i.e. save, check, rename
bool Contact::save(QString path, Encryption *enc)
{

  if (isnull || !enc) {
      // ERROR
      dbg("ERROR: Contact record is null or encryption not configured") ;
      return false ;
  }

  if (!isempty && todo.isdirty()) todo.save(path + filedata[ID], enc) ;
  if (!isempty && history.isdirty()) history.save(path + filedata[ID]) ;

  if (isdirty) {

      QByteArray data ;
      QTextStream out(&data, QIODevice::WriteOnly);
      out.setCodec("UTF-8") ;
      out << "[contact]\n" ;
      for (int entry=0; entry<Contact::NumberOfRecords; entry++) {
          if (contactrecordinfo[entry].issaved) {
              QString entrytext = filedata[entry].trimmed().replace("\n","\\n");
              out << contactRecordName((enum ContactRecord)entry) << "=" << entrytext << "\n" ;
          }
      }

      out.flush();

      if (gConf->encryptedEnabled()) {
          dbg(QString("Saving {%1} as zcontact").arg(filedata[ID])) ;
          if (!enc->save(path + filedata[ID] + ".zcontact", data)) {
              dbg("failed") ;
              return false ;
          }
      } else {
          dbg(QString("Saving {%1} as contact").arg(filedata[ID])) ;
          QFile file(path + filedata[ID] + ".contact");
          if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
              dbg("failed") ;
              return false;
          }
          bool success = (file.write(data) == data.length()) ;
          file.close() ;
          return success ;
      }

      isnew = false ;
      isdirty = false ;
  }
  return true ;
}


bool Contact::isNew()
{
    return isnew ;
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

// TODO: CHECK CALLS TO GETFIELD WITH GROUP - THESE ARE NOW BOOLEAN
QString& Contact::getField(enum Contact::ContactRecord field)
{
    static QString nullstring="" ;

    if (!contactrecordinfook) {
        nullstring = "ERROR: ContactRecord Structure Invalid (contact.h)" ;
        return nullstring ;
    }

    if ((field<0 || field>=NumberOfRecords)) return nullstring ;
    else return filedata[field] ;
}

bool Contact::sortPhoneNumbers()
{
    dbg("sortPhoneNumbers()") ;

    bool updated=false;
    if (!filedata[Phone2].isEmpty() || !filedata[Phone3].isEmpty() || !filedata[Phone4].isEmpty()) {
        int len = Phone4Title - Phone2Title + 1 ;
        for (int i=0;i<len; i++) {
            for (int j=0; j<(len-i-1);j++) {
                int j1t = j + Phone2Title ;
                int j1 = j + Phone2 ;
                int j2t = j + Phone2Title + 1 ;
                int j2 = j + Phone2 + 1 ;

                QString s1 = filedata[j1t]+filedata[j1] ;
                QString s2 = filedata[j2t]+filedata[j2] ;
                if (!s2.isEmpty() && s1.compare(s2)>0) {
                    QString t1t = filedata[j1t] ;
                    QString t1 = filedata[j1] ;
                    filedata[j1t] = filedata[j2t] ;
                    filedata[j1] = filedata[j2] ;
                    filedata[j2t] = t1t ;
                    filedata[j2] = t1 ;
                    updated=true ;
                }
            }
        }
    }
    return updated ;
}

void Contact::setField(enum Contact::ContactRecord field, QString data)
{
    if (isnull) {
        // ERROR, attempting to set NULL Contact
        dbg(QString("ERROR: setField(%1, %2) of NULL contact").arg((int)field).arg(data)) ;
        return ;
    }

#ifdef TRAPID
    if (field==Contact::ID && data.compare(TRAPID)==0) {
        bool SettingTrapID = true ;
    }
    if (field==Contact::Surname && filedata[Contact::ID].compare(TRAPID)==0) {
        bool SettingSurnameForTrapID = true;
    }
#endif

    QString dat ;
    dat = data.replace("\r"," ") ;
    dat = dat.trimmed() ;

    if (field>=Contact::Phone && field<=Contact::Phone4)
        dat = parsePhoneNumber(dat) ;

    if (field==Contact::Birthday)
        dat = parseDate(dat) ;

    if (field>=Contact::Phone2Title  && field<=Contact::Phone4Title) {
        dat = dat.left(1).toUpper() + dat.mid(1).toLower() ;
    }

    if (field>=0 && field<Contact::NumberOfRecords && dat.compare(filedata[field])!=0) {

        // Store data
        filedata[field]=dat ;
        isempty=false ;
        isdirty = true ;
        getOverviewDirty = true ;
        getOverviewHtmlDirty = true ;

        // Update timestamp
        QDateTime now = QDateTime::currentDateTimeUtc() ;
        filedata[Contact::Updated] = now.toString() ;

    }


    // Build sortstring
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

    if ( !filedata[Names].isEmpty() && (field==Names || field==ID) &&
        filedata[Names].compare(filedata[ID])!=0) {
        int trap=1 ;
    }
    isnull = false ;
}

void Contact::setDate(enum ContactRecord field, QDateTime data)
{
    setField(field, data.toString()) ;
}

QDateTime& Contact::getDate(enum ContactRecord field)
{
    static QDateTime dt ;
    dt.setUtcOffset(0);
    dt = dt.fromString(getField(field)) ;
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
        if (result.left(1).compare("0")!=0 &&
                result.left(1).compare("*")!=0 &&
                result.length()>4 &&
                result.length()<10 &&
                !gConf->getLocalDiallingCode().isEmpty()) {

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
    QString phone3Title = getField(Phone3Title) + " Phone" ;
    QString phone4Title = getField(Phone4Title) + " Phone" ;
    if (!getField(Phone2).isEmpty()) response += titlestart + phone2Title + ": " + titleend + getField(Phone2) + lineend ;
    if (!getField(Phone3).isEmpty()) response += titlestart + phone3Title + ": " + titleend + getField(Phone3) + lineend ;
    if (!getField(Phone4).isEmpty()) response += titlestart + phone4Title + ": " + titleend + getField(Phone4) + lineend ;
    if (!getField(ProfilePhone).isEmpty()) response += titlestart + "Phone: " + titleend + getField(ProfilePhone) + lineend ;
    if (!getField(Email).isEmpty()) response += titlestart + " Primary Email:  " + titleend + getField(Email) + lineend ;
    if (!getField(Email2).isEmpty()) response += titlestart + "Second Email:  " + titleend + getField(Email2) + lineend ;
    if (!getField(ProfileEMail).isEmpty()) response += titlestart + " Profile Email:  " + titleend + getField(ProfileEMail) + lineend ;
    if (!getField(Webaddress).isEmpty()) response += titlestart + "Web Site:  " + titleend + getField(Webaddress) + lineend ;
    if (!getField(Organisation).isEmpty()) response += titlestart + "Organisation: " + titleend + getField(Organisation) + lineend ;
    if (!getField(Address).isEmpty()) {
        QString a = getField(Address) ;
        a.replace(",", "") ;
        a.replace("\n", ", ") ;
        response += titlestart + " Primary Address:  " + titleend + a + lineend ;
    }
    if (!getField(Address2).isEmpty()) {
        QString a = getField(Address2) ;
        a.replace(",", "") ;
        a.replace("\n", ", ") ;
        response += titlestart + "Second Address:  " + titleend + a + lineend ;
    }
    if (!getField(ProfileAddress).isEmpty()) {
        QString a = getField(ProfileAddress) ;
        a.replace(",", "") ;
        a.replace("\n", ", ") ;
        response += titlestart + " Profile Address:  " + titleend + a + lineend ;
    }
    if (!getField(Birthday).isEmpty()) {
        QString a = getField(Birthday) ;
        response += titlestart + "Birthday: " + titleend + a.replace("\n",lineend) + lineend ;
    }
    QString groups ;
    if (isSet(Contact::GroupBusiness)) { groups = groups + "Business " ; }
    if (isSet(Contact::GroupClient)) { groups = groups + "Client " ; }
    if (isSet(Contact::GroupFamily)) { groups = groups + "Family " ; }
    if (isSet(Contact::GroupFriend)) { groups = groups + "Friend " ; }
    if (isSet(Contact::GroupOther)) { groups = groups + "Other " ; }
    if ( groups.isEmpty()) { groups = "None" ; }

    if (!getField(ProfileSurname).isEmpty() || !getField(ProfileNames).isEmpty()) response += titlestart + " Profile Name: " + titleend + getField(ProfileNames) + " " + getField(ProfileSurname) + lineend ;

    response += titlestart + "Type: " + titleend + groups + lineend ;
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

// Merged ID
int Contact::mergedIdCount() { return mergedidlist.length() ; }
void Contact::appendMergedId(QString id) { mergedidlist.append(id) ; }
QStringList& Contact::mergedIdList() { return mergedidlist ; }

// test Operators


Contact& Contact::getThis()
{
    return *this ;
}


// Return true if i is of type mctype
bool Contact::isContactOfType(Contact::ContactRecord i, int mctype)
{
    bool testdetails = ( (mctype&Contact::mcDetails)!=0) ;
    bool testgroup =  ( (mctype&Contact::mcGroup)!=0) ;
    bool testid = ( (mctype&Contact::mcId)!=0) ;
    bool testgoogleid = ( (mctype&Contact::mcGoogleId)!=0) ;
    bool testflag = ( (mctype&Contact::mcFlag)!=0) ;
    bool testprofile = ( (mctype&Contact::mcProfile)!=0) ;
    bool testetag = ( ( mctype&Contact::mcEtag)!=0) ;
    bool testcontrol = ( ( mctype&Contact::mcControlFlags) !=0) ;

    return ( (testdetails && this->contactrecordinfo[(Contact::ContactRecord)i].isdetails) ||
         (testgroup && this->contactrecordinfo[(Contact::ContactRecord)i].isgroupdetails) ||
         (testid && i==(int)Contact::ID) ||
         (testgoogleid && i==(int)Contact::GoogleRecordId) ||
         (testflag && this->contactrecordinfo[(Contact::ContactRecord)i].isflag) ||
         (testprofile && this->contactrecordinfo[(Contact::ContactRecord)i].isprofile) ||
         (testetag && i==(int)Contact::GoogleEtag) ||
         (testcontrol && (i==(int)Contact::ToBeUploaded || i==(int)Contact::ToBeDownloaded))) ;
}

// Copy Fields to other
bool Contact::copyTo(Contact &other, int mctype)
{
    bool copied=false ;
    for (int i=0; i<Contact::NumberOfRecords; i++) {
        if ( isContactOfType((Contact::ContactRecord)i, mctype)) {
            other.setField((Contact::ContactRecord)i, getField((Contact::ContactRecord)i)) ;
            copied=true ;
        }
    }
    return copied ;
}

// Compare this with 'with' and return true/false
bool Contact::matches(Contact &with, int mctype)
{
    return mismatch(with, mctype).isEmpty() ;
}

// Compare this with 'with' and return description of differences
QString Contact::mismatch(Contact &with, int mctype, bool showboth)
{
    QString result = "" ;
    for (int i=0; i<Contact::NumberOfRecords; i++) {
        if ( isContactOfType((Contact::ContactRecord)i, mctype)) {
            QString thisfield = getField((Contact::ContactRecord)i) ;
            QString thatfield = with.getField((Contact::ContactRecord)i) ;
            if (thisfield.compare(thatfield)!=0) {
                if (!result.isEmpty()) result = result + QString(", ") ;
                result = result + with.contactRecordName((Contact::ContactRecord)i) + QString(": ") ;
                result = result + thisfield ;
                if (showboth) result = result + QString(" / ") + thatfield ;
            }
        }
    }
    return result ;
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

