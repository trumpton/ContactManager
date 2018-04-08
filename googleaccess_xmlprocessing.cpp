
#include "googleaccess.h"

//*****************************************************************************************************
//
// PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE
//
//   DOM Manipulation Helper Functions
//

QDomElement GoogleAccess::domSearchChild(QDomElement &node, QString tag, QString rellabel, int occurrence)
{
    QDomElement domNode = node.firstChildElement() ;

    while (!domNode.isNull()) {

        QString tn = domNode.tagName().replace(" ","") ;
        if (tn.compare(tag, Qt::CaseInsensitive)==0) {
            QString xmlrellabel="" ;

            if (domNode.hasAttribute("href")) xmlrellabel=domNode.attribute("href").replace(" ", "") ;
            if (domNode.hasAttribute("name")) xmlrellabel=domNode.attribute("name").replace(" ", "") ;
            if (domNode.hasAttribute("label")) xmlrellabel=domNode.attribute("label").replace(" ", "") ;
            if (domNode.hasAttribute("rel")) xmlrellabel=domNode.attribute("rel").replace(" ", "") ;

            if (rellabel==(char *)0 || xmlrellabel.compare(rellabel, Qt::CaseInsensitive)==0) {
                if (occurrence>1) occurrence-- ;
                else return domNode ;
            }

        }
        domNode = domNode.nextSiblingElement() ;
    }
    return domNode ;
}


//*****************************************************************************************************
//
// PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE PRIVATE
//
//   updateElement
//
//  Searches for given element, and changes it if it is different to value
//

bool GoogleAccess::updateElementAttribute(QDomDocument doc, QDomElement documentroot, char *thistagname, char *attribute, QString value, char *rellabel)
{
    return updateElementTextAttribute(doc, documentroot, thistagname, value, rellabel, attribute) ;
}

bool GoogleAccess::updateElementText(QDomDocument doc, QDomElement documentroot, char *thistagname, QString value, char *rellabel)
{
    return updateElementTextAttribute(doc, documentroot, thistagname, value, rellabel, (char *)0) ;
}

bool GoogleAccess::updateElementHrefFlag(QDomDocument doc, QDomElement documentroot, QString thistagname, QString attributevalue, bool set)
{
    QDomElement domThisTag = domSearchChild(documentroot, thistagname, attributevalue) ;

    if (!domThisTag.isNull() && !set) {
        documentroot.removeChild(domThisTag) ;
        return true ;
    }

    if (domThisTag.isNull() && set) {
        QDomElement newDomGroup = doc.createElement(thistagname) ;
        newDomGroup.setAttribute("href", attributevalue) ;
        documentroot.appendChild(newDomGroup) ;
        return true ;
    }

    return false ;
}

bool GoogleAccess::updateElementTextAttribute(QDomDocument doc, QDomElement documentroot, char *thistagname, QString value, char *rellabel, char *attribute)
{
    QDomElement domThisTag ;
    QString thisTagText ;

    // Find this entry and its data
    domThisTag = domSearchChild(documentroot, thistagname, rellabel) ;
    if (domThisTag.isNull()) {
        thisTagText="" ;
    } else {
        if (attribute == (char *)0) {
            thisTagText = domThisTag.text();
        } else {
            thisTagText = domThisTag.attribute(attribute) ;
        }
    }

    if (thisTagText.compare(value, Qt::CaseInsensitive)==0) {

        // No change required
        return false ;

    } else {

        // Remove current entry
        if (!domThisTag.isNull()) documentroot.removeChild(domThisTag) ;

        // Create new entry and set attribute / text
        if (!value.isEmpty()) {

            QDomElement newDomThisTag ;
            newDomThisTag = doc.createElement(thistagname) ;

            if (attribute == (char *)0) {
                QDomText newDomText ;
                newDomText = doc.createTextNode(value) ;
                newDomThisTag.appendChild(newDomText) ;
            } else {
                newDomThisTag.setAttribute(attribute, value) ;
            }

            // Add rel= or label= attribute
            if (rellabel!=(char *)0) {
                if (rellabel[0]=='h' && rellabel[1]=='t') {
                    newDomThisTag.setAttribute((char *)"rel", rellabel) ;
                } else {
                   newDomThisTag.setAttribute((char *)"label", rellabel) ;
                }
            }

            // Add to document
            documentroot.appendChild(newDomThisTag) ;

        }

    }

    return true ;
}

bool GoogleAccess::updateElementText2(QDomDocument doc, QDomElement documentroot, char *parenttagname, char *thistagname, QString value, char *rellabel)
{
    QDomElement domParentTag ;
    QString xml ;
    bool status=false ;

    xml = doc.toString() ;
    domParentTag = domSearchChild(documentroot, parenttagname) ;
    if (!domParentTag.isNull())
        documentroot.removeChild(domParentTag) ;

    if (value.isEmpty()) {
        status=false ;
    } else {
        QDomElement newDomParentTag = doc.createElement(parenttagname) ;
        documentroot.appendChild(newDomParentTag) ;

        // Add rel= or label= attribute
        if (rellabel!=(char *)0) {
            if (rellabel[0]=='h' && rellabel[1]=='t') {
                newDomParentTag.setAttribute((char *)"rel", rellabel) ;
            } else {
               newDomParentTag.setAttribute((char *)"label", rellabel) ;
            }
        }
        status=updateElementText(doc, newDomParentTag, thistagname, value, (char *)0) ;
    }

    xml = doc.toString() ;
    return status ;
}


