#ifndef FIELDFORMAT_H
#define FIELDFORMAT_H

#include <QString>

// Rules for the phone numbers and email addresses typed into the contact form, and for their labels
namespace FieldFormat {

    // The longest label, in characters
    const int MaxLabelLength = 30 ;

    // Takes a label off the end or start of an entry: "01234 567890 (Home)", "01234 567890 - Home" or "Home: 01234 567890".
    // Returns true only when both the value and the label are found, and then value and label hold them.
    bool splitLabel(const QString &text, QString &value, QString &label) ;

    // Digits, spaces, brackets, hyphens and dots, with an optional + at the start, and at least one digit
    bool validPhoneNumber(const QString &number) ;

    // One @ with text either side, and a dot in the domain
    bool validEmailAddress(const QString &address) ;

    // Any printable character except = and backslash, up to MaxLabelLength characters
    bool validLabel(const QString &label) ;

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
