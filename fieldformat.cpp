#include "fieldformat.h"

#include <QRegularExpression>
#include <QStringList>

namespace FieldFormat {

// A label typed after a number or address cannot contain the delimiters (: - and brackets), and must start with a letter
static const QString labelpattern = QStringLiteral("\\p{L}[^\\x{0}-\\x{1F}=\\\\:()\\-]*") ;

bool splitLabel(const QString &text, QString &value, QString &label)
{
    static const QRegularExpression bracketed(QStringLiteral("^(.+?)\\s*\\(\\s*(%1)\\s*\\)$").arg(labelpattern)) ;
    static const QRegularExpression dashed(QStringLiteral("^(.+?)\\s+-\\s*(%1)$").arg(labelpattern)) ;
    static const QRegularExpression prefixed(QStringLiteral("^(%1)\\s*:\\s*(.+)$").arg(labelpattern)) ;

    QString trimmed = text.trimmed() ;
    QRegularExpressionMatch m = bracketed.match(trimmed) ;
    QString v, l ;
    if (m.hasMatch()) {
        v = m.captured(1) ;
        l = m.captured(2) ;
    } else if ((m = dashed.match(trimmed)).hasMatch()) {
        v = m.captured(1) ;
        l = m.captured(2) ;
    } else if ((m = prefixed.match(trimmed)).hasMatch()) {
        l = m.captured(1) ;
        v = m.captured(2) ;
    } else {
        return false ;
    }

    v = v.trimmed() ;
    l = l.trimmed() ;
    if (v.isEmpty() || l.isEmpty()) return false ;

    value = v ;
    label = l ;
    return true ;
}

bool validPhoneNumber(const QString &number)
{
    static const QRegularExpression allowed(QStringLiteral("^\\+?[0-9 ()\\-.]+$")) ;
    QString n = number.trimmed() ;
    if (!allowed.match(n).hasMatch()) return false ;

    int digits = 0 ;
    for (const QChar c : n) if (c.isDigit()) digits++ ;
    return digits>0 ;
}

bool validEmailAddress(const QString &address)
{
    static const QRegularExpression email(QStringLiteral("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$")) ;
    return email.match(address.trimmed()).hasMatch() ;
}

// Any printable character Google accepts as a custom label, except the ones the contact file cannot hold: '=' and '\\'
bool validLabel(const QString &label)
{
    QString l = label.trimmed() ;
    if (l.isEmpty() || l.length()>MaxLabelLength) return false ;
    for (const QChar c : l) {
        if (!c.isPrint() || c==QChar('=') || c==QChar('\\')) return false ;
    }
    return true ;
}

Tidied tidyEntry(const QString &text, bool isEmail)
{
    Tidied r ;
    r.fixed = true ;

    QString first, firstlabel, second, secondlabel ;
    QString value = text.trimmed() ;
    QString label ;
    if (splitLabel(text, first, firstlabel)) {
        if (splitLabel(first, second, secondlabel)) {
            r.fixed = false ;
            r.problem = QStringLiteral("it has more than one label") ;
            r.value = text.trimmed() ;
            return r ;
        }
        value = first.trimmed() ;
        label = firstlabel ;
    }

    if (!label.isEmpty() && !validLabel(label)) {
        r.fixed = false ;
        r.problem = QString("the label \"%1\" is not allowed").arg(label) ;
    } else if (!value.isEmpty()) {
        bool ok = isEmail ? validEmailAddress(value) : validPhoneNumber(value) ;
        if (!ok) {
            r.fixed = false ;
            r.problem = QString("\"%1\" is not a valid %2").arg(value, isEmail ? QString("email address") : QString("phone number")) ;
        }
    }

    if (!r.fixed) {
        r.value = text.trimmed() ;
        return r ;
    }

    r.value = value ;
    r.label = label.isEmpty() ? QString() : normaliseLabel(label) ;
    return r ;
}

QString normaliseLabel(const QString &label)
{
    QStringList words = label.trimmed().split(' ', Qt::SkipEmptyParts) ;
    for (QString &word : words) word = word.left(1).toUpper() + word.mid(1).toLower() ;
    return words.join(' ') ;
}

}
