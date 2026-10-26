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

    // Starts with a letter, then letters, digits, spaces, hyphens or apostrophes, up to MaxLabelLength characters
    bool validLabel(const QString &label) ;

    // The first letter of each word in capitals, the rest in lower case
    QString normaliseLabel(const QString &label) ;
}

#endif // FIELDFORMAT_H
