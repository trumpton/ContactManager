#ifndef FIELDFORMAT_H
#define FIELDFORMAT_H

#include <QString>

// Rules for the phone numbers and email addresses typed into the contact form, and for their labels
namespace FieldFormat {

    // The longest label, in characters
    const int MaxLabelLength = 20 ;

    // Takes a label off the end or start of an entry: "01234 567890 (Home)", "01234 567890 - Home" or "Home: 01234 567890".
    // Returns true only when both the value and the label are found, and then value and label hold them.
    // For a phone number the hyphen may have no space before it, as the number is stored with its spaces removed.
    // An email address never splits at a hyphen with no space, as user-name@host.com is one address.
    bool splitLabel(const QString &text, QString &value, QString &label, bool phone=false) ;

    // Drops the trunk zero in brackets after a country code, so "+44 (0) 1234 567890" becomes "+44 1234 567890".
    // Many mobile phones do not accept the form with it.
    QString dropTrunkZero(const QString &number) ;

    // Digits, spaces, brackets, hyphens and dots, with an optional + at the start, and at least one digit
    bool validPhoneNumber(const QString &number) ;

    // One @ with text either side, and a dot in the domain
    bool validEmailAddress(const QString &address) ;

    // Any printable character except colon, slash, backslash and =, with no tabs or line breaks, up to MaxLabelLength characters
    bool validLabel(const QString &label) ;

    // A number or address that Google holds but no slot is free for.  It is kept in Comments as a line
    // "Also on Google (Type): value", so the upload can send it back and Google keeps it.
    struct AlsoOnGoogle {
        QString type ;
        QString value ;
    } ;
    QString alsoOnGoogleLine(const QString &type, const QString &value) ;
    QList<AlsoOnGoogle> alsoOnGoogleLines(const QString &comments) ;

    // The first letter of each word in capitals, the rest in lower case
    QString normaliseLabel(const QString &label) ;

    // The result of checking one email or phone entry, and of putting it right where that can be done
    struct Tidied {
        QString value ;     // the address or number, without its label
        QString label ;     // the label found in the entry, normalised; empty when there is none
        bool fixed ;        // false when the entry cannot be put right, and should be left as it is
        QString problem ;   // why it cannot be, when fixed is false
    } ;

    // Checks an email address or phone number and takes off a label typed into it, as in "user@host.com (Work)".  An entry
    // with more than one label, or one that is not valid once its label is taken off, is not fixed.
    Tidied tidyEntry(const QString &text, bool isEmail) ;
}

#endif // FIELDFORMAT_H
