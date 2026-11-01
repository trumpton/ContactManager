
#include "mainwindow.h"
#include "help.h"
#include "../Lib/alertsound.h"
#include <QAccessible>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMetaEnum>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

// HELP MENU


void MainWindow::on_actionAbout_triggered()
{
    class About *abt ;
    abt = new About(this) ;    
    play(Ok) ;
    abt->exec() ;
    delete abt ;
}


void MainWindow::on_actionManual_triggered()
{
    class Help *hlp ;
    hlp = new Help(this) ;
    play(Ok) ;
    hlp->exec() ;
    delete hlp ;
}


//
// on_actionShowAccessibleText_triggered
//
// Help > Show Screen Reader Text (issue #101).  A dialog shows the name, role, value and description that the control with the
// focus is given for a screen reader.  A screen reader builds its speech and braille from these, adding its own wording, so the
// text is an approximation.  For the calendar list, the current entry is shown as well.  The text follows the focus, and changes
// as values change.
//
static QString accessibleRoleName(QAccessible::Role role)
{
    QMetaEnum metaenum = QMetaEnum::fromType<QAccessible::Role>() ;
    const char *key = metaenum.isValid() ? metaenum.valueToKey((int)role) : nullptr ;
    return key ? QString(key) : QString::number((int)role) ;
}

// A value that is empty is shown as (none), so that it is not lost on a braille display
static QString orNone(const QString &text)
{
    return text.isEmpty() ? QString("(none)") : text ;
}

void MainWindow::updateAccessibleInfo()
{
    if (accessibleinfo.isNull()) return ;
    QPlainTextEdit *edit = accessibleinfo->findChild<QPlainTextEdit*>("AccessibleInfoText") ;
    if (!edit) return ;

    QString text ;
    QWidget *target = accessibleinfotarget ;
    if (!target) {
        text = "Move the focus to a control in Contact Manager, and its screen reader text is shown here." ;
    } else {
        QString classname = QString(target->metaObject()->className()) ;
        QString control = target->objectName().isEmpty() ? classname : target->objectName() + " (" + classname + ")" ;
        text = "Control: " + control ;

        QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(target) ;
        if (!iface) {
            text += "\nNo screen reader text for this control." ;
        } else {
            text += "\nName: " + orNone(iface->text(QAccessible::Name)) ;
            text += "\nRole: " + accessibleRoleName(iface->role()) ;
            text += "\nValue: " + orNone(iface->text(QAccessible::Value)) ;
            text += "\nDescription: " + orNone(iface->text(QAccessible::Description)) ;
        }

        // The calendar list has an entry as its current item, which is what a screen reader reads
        if (target==ui->listCalendar) {
            QModelIndex current = ui->listCalendar->currentIndex() ;
            if (current.isValid() && current.row()<calendarrowstarts.size()) {
                text += "\nCurrent entry: " + orNone(calendarlist.data(current, Qt::AccessibleTextRole).toString()) ;
                text += QString("\nEntry %1 of %2").arg(current.row()+1).arg(calendarrowstarts.size()) ;
            }
        }
    }

    // Only a change is set, as setting the text again would move the reader's place in it
    if (edit->toPlainText()!=text) edit->setPlainText(text) ;
}

void MainWindow::on_actionShowAccessibleText_triggered()
{
    if (!accessibleinfo.isNull()) {
        accessibleinfo->raise() ;
        accessibleinfo->activateWindow() ;
        return ;
    }

    // The control that had the focus is the one described, as the dialog takes the focus when it opens
    accessibleinfotarget = QApplication::focusWidget() ;

    QDialog *dialog = new QDialog(this) ;
    dialog->setObjectName("AccessibleInfoDialog") ;
    dialog->setWindowTitle("Screen Reader Text") ;
    dialog->setAttribute(Qt::WA_DeleteOnClose) ;

    QVBoxLayout *layout = new QVBoxLayout(dialog) ;
    QPlainTextEdit *edit = new QPlainTextEdit(dialog) ;
    edit->setObjectName("AccessibleInfoText") ;
    edit->setReadOnly(true) ;
    edit->setAccessibleName("Screen reader text") ;
    layout->addWidget(edit) ;

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog) ;
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject) ;
    layout->addWidget(buttons) ;

    accessibleinfo = dialog ;

    // The control described follows the focus, except when the focus is in this dialog.  The connection ends with the dialog.
    QObject::connect(qApp, &QApplication::focusChanged, dialog, [this](QWidget *, QWidget *now) {
        if (!now || accessibleinfo.isNull()) return ;
        if (now==accessibleinfo || accessibleinfo->isAncestorOf(now)) return ;
        accessibleinfotarget = now ;
        updateAccessibleInfo() ;
    }) ;

    // A value changes as text is typed, and the current calendar entry changes without the focus moving, so the text is refreshed
    QTimer *timer = new QTimer(dialog) ;
    QObject::connect(timer, &QTimer::timeout, dialog, [this]() { updateAccessibleInfo() ; }) ;
    timer->start(300) ;

    updateAccessibleInfo() ;
    dialog->show() ;
}
