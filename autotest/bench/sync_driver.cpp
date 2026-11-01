//
// Headless driver for the Google bench (autotest/google_bench.py).
//
// Starts ContactManager without its single-instance check, runs the operations in a JSON file in order,
// and writes a dump of the local contacts and appointments.  Dialogs are answered as the headless driver
// answers them: the encryption key and login use CM_TEST_SECRET, every other dialog takes its default.
//
// Test builds only.  The main window's private members are reached with '#define private public', which
// must not be used in the application itself.
//
// usage: sync_driver <outdir> <operations.json>
//
// Superset of bench_driver.cpp: adds the operations the local-file checks use (listdir, paths, dumplog, setting,
// wait, key and the list-navigation operations).  Built by build_driver.sh.
//
#include <QApplication>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QTest>
#include <QAction>
#include <QCloseEvent>
#include <QDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollBar>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QProcess>
#include <QKeyEvent>
#include "warningchoice.h"
#include <functional>
#include <cstdlib>
#include "../Lib/alertsound.h"
#define private public
#include "mainwindow.h"
#include "appointmentform.h"
#include "../Lib/accessibledaterangeedit.h"
#undef private
#include "contact.h"
#include "contactdatabase.h"
#include <QInputDialog>
#include <QLineEdit>
#include "calendar.h"
#include "appointment.h"

static QString outdir ;
static QStringList dateAnswers ;   // the text each date entry is answered with, in order; empty cancels it (issue #92)
static int dialogCount = 0 ;
static QString pendingChoice ;     // the letter the next choice dialog is answered with (empty cancels it)
static int syncCount = 0 ;

// Values the operations report back to the bench, in results.json in the run directory
static QJsonObject results ;

static void record(const QString &key, const QJsonValue &value)
{
    results.insert(key, value) ;
    QFile f(outdir + "/results.json") ;
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(results).toJson()) ;
}

static void note(const QString &text)
{
    QFile f(outdir + "/driver.log") ;
    if (f.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << QDateTime::currentDateTime().toString("hh:mm:ss") << " " << text << "\n" ;
}

static QPushButton *defaultButton(QWidget *dialog)
{
    QPushButton *first = nullptr ;
    for (QPushButton *b : dialog->findChildren<QPushButton*>()) {
        if (!b->isVisible() || !b->isEnabled()) continue ;
        if (b->isDefault()) return b ;
        if (!first) first = b ;
    }
    return first ;
}

static QPushButton *okButton(QWidget *dialog)
{
    auto *box = dialog->findChild<QDialogButtonBox*>() ;
    return box ? box->button(QDialogButtonBox::Ok) : nullptr ;
}

// Contact record number for a field name such as "names" or "email3title"
static int contactRecordFor(const QString &key)
{
    Contact probe ;
    for (int i = 0; i < (int)Contact::NumberOfRecords; i++) {
        if (QString(probe.contactRecordName((Contact::ContactRecord)i)) == key) return i ;
    }
    return -1 ;
}

// Appointment fields the bench sets or reads
static int appointmentRecordFor(const QString &key)
{
    if (key == "summary") return Appointment::Summary ;
    if (key == "description") return Appointment::Description ;
    if (key == "location") return Appointment::Location ;
    if (key == "from") return Appointment::From ;
    if (key == "to") return Appointment::To ;
    if (key == "for") return Appointment::For ;
    if (key == "googlerecordid") return Appointment::GoogleRecordId ;
    if (key == "repeat") return Appointment::Repeat ;
    if (key == "repeatinterval") return Appointment::RepeatInterval ;
    if (key == "repeatcount") return Appointment::RepeatCount ;
    if (key == "repeatuntil") return Appointment::RepeatUntil ;
    return -1 ;
}

// The entries on the contact details form, by contact field name
static QMap<QString, QLineEdit*> formEntries(Ui::MainWindow *ui)
{
    QMap<QString, QLineEdit*> m ;
    m["names"] = ui->editFirstName ;
    m["surname"] = ui->editSurname ;
    m["voip"] = ui->editVoip ;
    m["email"] = ui->editEmail ;
    m["email2"] = ui->editEmail2 ;
    m["email3"] = ui->editEmail3 ;
    m["email4"] = ui->editEmail4 ;
    m["phone"] = ui->editHome ;
    m["work"] = ui->editWork ;
    m["mobile"] = ui->editMobile ;
    m["phone2"] = ui->edit2 ;
    m["phone3"] = ui->edit3 ;
    m["phone4"] = ui->edit4 ;
    return m ;
}

static void applyContactFields(Contact &c, const QJsonObject &fields)
{
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        int rec = contactRecordFor(it.key()) ;
        if (rec < 0) {
            note("unknown contact field: " + it.key()) ;
            continue ;
        }
        c.setField((Contact::ContactRecord)rec, it.value().toString()) ;
    }
    c.markAsDirty() ;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv) ;
    a.setApplicationName("Contact Manager") ;
    outdir = QString::fromLocal8Bit(argv[1]) ;

    QFile opsfile(QString::fromLocal8Bit(argv[2])) ;
    if (!opsfile.open(QIODevice::ReadOnly)) {
        note("cannot read operations file") ;
        return 2 ;
    }
    QJsonArray ops = QJsonDocument::fromJson(opsfile.readAll()).array() ;
    note(QString("driver started, %1 operations").arg(ops.size())) ;

    // Answer dialogs as they appear, including any shown while the main window is being built
    QWidget *lastmodal = nullptr ;
    int clicksonthis = 0 ;
    QString nextanswer ;     // The button the next yes/no question is answered with, set by an "answer" operation
    QList<QDateTime> nextfroms ;   // The start each appointment form is given when it is shown, then OK is pressed
    QTimer *tick = new QTimer(&a) ;
    QObject::connect(tick, &QTimer::timeout, [&]() {
        QWidget *m = QApplication::activeModalWidget() ;
        if (!m) return ;

        // An appointment form: give it the next start, as a user types one, and press OK
        if (QString(m->metaObject()->className()) == "AppointmentForm" && !nextfroms.isEmpty()) {
            auto *from = m->findChild<AccessibleDateRangeEdit*>("editWhenFrom") ;
            auto *to = m->findChild<AccessibleDateRangeEdit*>("editWhenTo") ;
            if (from && to) {
                // The end moves with the start, keeping the hour the form has
                QDateTime start = nextfroms.takeFirst() ;
                from->setDateTime(start, 60) ;
                to->setDateTime(start.addSecs(3600)) ;
                note("form start set") ;
            }
            for (QPushButton *b : m->findChildren<QPushButton*>()) {
                if (b->text().replace("&", "") == "OK") {
                    b->click() ;
                    return ;
                }
            }
        }

        if (!nextanswer.isEmpty()) {
            for (QPushButton *b : m->findChildren<QPushButton*>()) {
                if (b->text() == nextanswer) {
                    note("answered \"" + nextanswer + "\": " + m->windowTitle()) ;
                    nextanswer.clear() ;
                    b->click() ;
                    return ;
                }
            }
        }

        // A long sync keeps its progress dialog open for a long time, so only other dialogs can be stuck
        if (m->objectName() != "GoogleUpdateDialog" && m == lastmodal) {
            if (++clicksonthis > 40) {
                note("giving up: dialog did not close: " + m->windowTitle()) ;
                QApplication::quit() ;
            }
        } else {
            lastmodal = m ;
            clicksonthis = 0 ;
        }

        if (m->objectName() == "GoogleUpdateDialog") {
            auto *report = m->findChild<QTextEdit*>("updateStatusReport") ;
            if (report) {
                QFile f(outdir + QString("/report%1.txt").arg(syncCount)) ;
                if (f.open(QIODevice::WriteOnly | QIODevice::Text))
                    QTextStream(&f) << report->toPlainText() ;
            }
            for (QPushButton *b : m->findChildren<QPushButton*>()) {
                if (b->text().startsWith("Update Complete") || b->text().startsWith("Update Failed")) {
                    note(QString("sync %1 finished: %2").arg(syncCount).arg(b->text())) ;
                    b->click() ;
                    return ;
                }
                // Tidy Contacts and Import end on a summary, which waits for Close
                if (b->text() == "Close") {
                    note("progress window closed: " + m->windowTitle()) ;
                    b->click() ;
                    return ;
                }
            }
            return ;
        }

        if (QString(m->metaObject()->className()) == "Encryption") {
            QString secret = QString::fromLocal8Bit(qgetenv("CM_TEST_SECRET")) ;
            if (secret.isEmpty() || (m->windowTitle() != "Enter Key and Password" && m->windowTitle() != "Login")) {
                note("stopping at unexpected key dialog: " + m->windowTitle()) ;
                QApplication::quit() ;
                return ;
            }
            QStringList fields = (m->windowTitle() == "Login") ? QStringList() << "lineEdit2"
                                                                : QStringList() << "lineEdit0" << "lineEdit1" << "lineEdit2" << "lineEdit3" ;
            for (const QString &name : fields) m->findChild<QLineEdit*>(name)->setText(secret) ;
            okButton(m)->click() ;
            return ;
        }

        // The main window can be the active modal widget while login is finishing: leave it alone
        if (QString(m->metaObject()->className()) == "MainWindow") return ;

        // A date entry (QInputDialog) is recorded, then answered with the next of dateAnswers, or cancelled
        if (auto *input = qobject_cast<QInputDialog*>(m)) {
            QLineEdit *edit = input->findChild<QLineEdit*>() ;
            QString answer = dateAnswers.isEmpty() ? QString() : dateAnswers.takeFirst() ;
            dialogCount++ ;
            QString prefix = QString("dialog_%1").arg(dialogCount) ;
            if (edit) {
                record(prefix + "_text", QJsonValue(edit->text())) ;
                record(prefix + "_cursor", QJsonValue(edit->cursorPosition())) ;
                record(prefix + "_length", QJsonValue(edit->text().length())) ;
                record(prefix + "_selected", QJsonValue(edit->selectedText().length())) ;
                record(prefix + "_accname", QJsonValue(edit->accessibleName())) ;
                record(prefix + "_accdesc", QJsonValue(edit->accessibleDescription())) ;
            }
            record(prefix + "_title", QJsonValue(input->windowTitle())) ;
            if (answer.isEmpty() || !edit) {
                note("date entry cancelled") ;
                input->reject() ;
            } else {
                note("date entry answered") ;
                edit->setText(answer) ;
                QTest::keyClick(edit, Qt::Key_Return) ;
            }
            return ;
        }

        // A choice dialog is answered with the letter set by "choose", or cancelled when none is set
        if (auto *choice = qobject_cast<WarningChoice*>(m)) {
            QString letter = pendingChoice ;
            pendingChoice.clear() ;
            note(QString("choice dialog: %1").arg(letter.isEmpty() ? QString("cancelled") : letter)) ;
            if (letter.isEmpty()) {
                choice->reject() ;
            } else {
                QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, letter) ;
                QApplication::sendEvent(choice, &press) ;
            }
            return ;
        }

        QString text ;
        if (auto *box = qobject_cast<QMessageBox*>(m)) text = box->text() ;
        note(QString("dialog %1 \"%2\": %3").arg(m->metaObject()->className(), m->windowTitle(), text)) ;
        QPushButton *b = defaultButton(m) ;
        if (b) b->click() ;
        else m->close() ;
    }) ;
    tick->start(300) ;

    MainWindow w ;
    initSound(NULL) ;
    w.show() ;
    note("main window shown") ;

    int next = 0 ;
    std::function<void()> step ;
    step = [&]() {
        if (next >= ops.size()) {
            note("operations finished") ;
            QTimer::singleShot(500, &a, [&]() {
                // A refused entry is still held on the form.  Exit now, as the results are already written, rather
                // than saving the form again while the window is destroyed.
                if (w.titledEntriesHeld) {
                    note("exiting with a refused entry on the form") ;
                    std::_Exit(0) ;
                }
                QApplication::quit() ;
            }) ;
            return ;
        }
        QJsonObject op = ops[next++].toObject() ;
        QString kind = op["op"].toString() ;
        note(QString("op %1: %2").arg(next).arg(kind)) ;

        if (kind == "sync") {
            syncCount++ ;
            QAction *act = w.findChild<QAction*>("action_Google_Sync") ;
            if (act) act->trigger() ;          // returns when the sync dialog has closed
            else note("action_Google_Sync not found") ;
            note(QString("sync %1 returned").arg(syncCount)) ;

        } else if (kind == "add_contact") {
            Contact &c = w.db.addContact() ;
            applyContactFields(c, op["fields"].toObject()) ;
            note("added contact " + c.getField(Contact::ID)) ;

        } else if (kind == "edit_contact" || kind == "delete_contact" || kind == "hide_contact") {
            QJsonObject match = op["match"].toObject() ;
            int hit = 0 ;
            for (int i = 0; i < w.db.size(); i++) {
                Contact &c = w.db.getContact(i) ;
                bool same = !match.isEmpty() ;
                for (auto it = match.begin(); it != match.end(); ++it) {
                    int rec = contactRecordFor(it.key()) ;
                    if (rec < 0 || c.getField((Contact::ContactRecord)rec) != it.value().toString()) same = false ;
                }
                if (!same) continue ;
                hit++ ;
                if (kind == "edit_contact") applyContactFields(c, op["fields"].toObject()) ;
                if (kind == "delete_contact") { c.setFlag(Contact::Deleted, true) ; c.setFlag(Contact::Hidden, true) ; }
                if (kind == "hide_contact") c.setFlag(Contact::Hidden, true) ;
            }
            note(QString("%1 matched %2 contact(s)").arg(kind).arg(hit)) ;

        } else if (kind == "add_appointment") {
            Appointment appt ;
            QJsonObject fields = op["fields"].toObject() ;
            for (auto it = fields.begin(); it != fields.end(); ++it) {
                int rec = appointmentRecordFor(it.key()) ;
                if (rec >= 0) appt.setField((Appointment::AppointmentRecord)rec, it.value().toString()) ;
            }
            // "surname" links the appointment to that contact, with its ID and name, as the form does
            QString surname = op["surname"].toString() ;
            for (int i = 0; !surname.isEmpty() && i < w.db.size(); i++) {
                Contact &c = w.db.getContact(i) ;
                if (c.getField(Contact::Surname) == surname) {
                    appt.setField(Appointment::ContactId, c.getField(Contact::ID)) ;
                    appt.setField(Appointment::For, c.getField(Contact::Names) + " " + surname) ;
                    break ;
                }
            }
            // An appointment with no contact is "Unnamed Contact", as the form sets it, and Google returns it that way
            if (appt.getField(Appointment::For).isEmpty()) appt.setField(Appointment::For, QString("Unnamed Contact")) ;
            // A repeat made in the form has the zone of this machine, as the form sets it
            if (appt.isRepeating() && appt.getField(Appointment::RepeatZone).isEmpty())
                appt.setField(Appointment::RepeatZone, QTimeZone::systemTimeZoneId()) ;
            w.calendar.addAppointment(appt, true) ;
            note("added appointment " + appt.getField(Appointment::Summary)) ;

        } else if (kind == "edit_appointment" || kind == "delete_appointment") {
            QString summary = op["match"].toObject()["summary"].toString() ;
            int hit = 0 ;
            for (int i = 0; i < w.calendar.size(); i++) {
                Appointment &appt = w.calendar.getAppointment(i) ;
                if (appt.getField(Appointment::Summary) != summary) continue ;
                hit++ ;
                if (kind == "delete_appointment") {
                    appt.setFlag(Appointment::Deleted, true) ;
                } else {
                    QJsonObject fields = op["fields"].toObject() ;
                    for (auto it = fields.begin(); it != fields.end(); ++it) {
                        int rec = appointmentRecordFor(it.key()) ;
                        if (rec >= 0) appt.setField((Appointment::AppointmentRecord)rec, it.value().toString()) ;
                    }
                }
            }
            note(QString("%1 matched %2 appointment(s)").arg(kind).arg(hit)) ;

        } else if (kind == "exec") {
            // Changes made on Google between syncs, by an external script
            QStringList args ;
            for (const QJsonValue &v : op["args"].toArray()) args << v.toString() ;
            int code = QProcess::execute(op["program"].toString(), args) ;
            note(QString("exec exit %1").arg(code)) ;

        } else if (kind == "open_contact") {
            // Opens the contact on the contact details form, as selecting it in the list does
            QJsonObject match = op["match"].toObject() ;
            QString id ;
            int hit = 0 ;
            for (int i = 0; i < w.db.size(); i++) {
                Contact &c = w.db.getContact(i) ;
                bool same = !match.isEmpty() ;
                for (auto it = match.begin(); it != match.end(); ++it) {
                    int rec = contactRecordFor(it.key()) ;
                    if (rec < 0 || c.getField((Contact::ContactRecord)rec) != it.value().toString()) same = false ;
                }
                if (!same) continue ;
                hit++ ;
                id = c.getField(Contact::ID) ;
            }
            if (hit == 1) {
                w.populateDialog(id, true) ;
                // "details" false selects the contact without opening its details, as a click in the list does
                if (op["details"].toBool(true)) w.on_actionEnableContactDetailsEdit_triggered() ;      // the form is open for editing, as when a user edits it
            }
            // The contact that is selected after the open, so a scenario can see whether the switch happened
            record(op["name"].toString() + "_selected", w.db.getSelected().getField(Contact::Surname)) ;
            // Where focus is after the open, so a scenario can see whether it stays on the calendar
            QWidget *focus = w.focusWidget() ;
            record(op["name"].toString() + "_focus", focus ? focus->objectName() : QString()) ;
            note(QString("open_contact matched %1 contact(s)").arg(hit)) ;

        } else if (kind == "alt_edit") {
            // Alt-E,D: opens Contact Details for the selected contact.  Records whether the page is showing afterwards.
            QString name = op["name"].toString() ;
            w.on_actionEnableContactDetailsEdit_triggered() ;
            record(name + "_details", w.ui->tabBar->currentIndex() == CONTACTDETAILSTABPOS) ;
            note("alt_edit " + name) ;

        } else if (kind == "calendar_click") {
            // Double-clicks a calendar entry: its contact is opened by the same handler the list uses.  The entry is first
            // linked to the contact with this surname, as an entry is linked to a contact when it is made.
            QString name = op["name"].toString() ;
            QString summary = op["summary"].toString() ;
            QString contactid, apptid ;
            for (int i = 0; i < w.db.size(); i++) {
                Contact &c = w.db.getContact(i) ;
                if (c.getField(Contact::Surname) == op["surname"].toString()) contactid = c.getField(Contact::ID) ;
            }
            for (int i = 0; i < w.calendar.size(); i++) {
                Appointment &appt = w.calendar.getAppointment(i) ;
                if (appt.getField(Appointment::Summary) == summary) {
                    appt.setContactId(contactid) ;
                    apptid = appt.getField(Appointment::ID) ;
                }
            }

            // The list is built when the calendar tab loads, so reload it to show the entry added since
            w.LoadCalendarTab() ;

            // The row of the entry, or of one of its repeats when an occurrence is given, with the calendar tab shown and
            // its list focused, as a click leaves it
            QString wanted = apptid ;
            if (op.contains("occurrence")) {
                wanted = Calendar::occurrenceHint(apptid, QDateTime::fromString(op["occurrence"].toString(), Qt::ISODate)) ;
            }
            int row = -1 ;
            for (int r = 0; r < w.calendarlist.rowCount(); r++) {
                if (w.calendarlist.hintAt(r) == wanted) row = r ;
            }
            record(name + "_expected_row", row) ;
            w.ui->tabBar->setCurrentIndex(CALENDARTABPOS) ;
            w.ui->listCalendar->setFocus() ;
            if (row >= 0) w.ui->listCalendar->setCurrentIndex(w.calendarlist.index(row, 0)) ;

            w.on_listCalendar_activated(w.ui->listCalendar->currentIndex()) ;

            QWidget *focus = w.focusWidget() ;
            record(name + "_row", w.ui->listCalendar->currentIndex().row()) ;
            record(name + "_focus", focus ? focus->objectName() : QString()) ;
            record(name + "_selected", w.db.getSelected().getField(Contact::Surname)) ;
            note(QString("calendar_click %1: row %2, focus %3").arg(name).arg(w.ui->listCalendar->currentIndex().row())
                 .arg(focus ? focus->objectName() : QString())) ;

        } else if (kind == "date_answers") {
            // The answers to the next date entries, in order: the text typed, or empty to cancel
            dateAnswers.clear() ;
            for (const QJsonValue &v : op["answers"].toArray()) dateAnswers.append(v.toString()) ;

        } else if (kind == "call") {
            // Calls a navigation function directly, then records where the list's current row is (issue #92 probe)
            QString name = op["name"].toString() ;
            QString fn = op["fn"].toString() ;
            // The list takes the current row only while it has the focus, so the window is made active first, as a user has it
            w.activateWindow() ; QApplication::setActiveWindow(&w) ; w.ui->listCalendar->setFocus() ;
            record(name + "_hasfocus", QJsonValue(w.ui->listCalendar->hasFocus())) ;
            if (fn=="gotodate") w.gotoCalendarDate(QDate(op["y"].toInt(), op["m"].toInt(), op["d"].toInt())) ;
            else if (fn=="today") w.on_actionGotoToday_triggered() ;
            else if (fn=="showcal") w.ui->tabBar->setCurrentIndex(1) ;
            else if (fn=="showacc") w.on_actionShowAccessibleText_triggered() ;
            else if (fn=="contact") w.populateDialog(op["id"].toString()) ;
            else if (fn=="raw") w.ui->listCalendar->setCurrentIndex(w.calendarlist.index(op["row"].toInt(),0)) ;
            record(name + "_selrow", QJsonValue(w.ui->listCalendar->selectionModel() ? w.ui->listCalendar->selectionModel()->currentIndex().row() : -99)) ;
            record(name + "_samemodel", QJsonValue(w.ui->listCalendar->model()==&w.calendarlist)) ;
            record(name + "_visible", QJsonValue(w.ui->listCalendar->isVisible())) ;
            record(name + "_tabcount", QJsonValue(w.ui->tabBar->count())) ;
            int cur = w.ui->listCalendar->currentIndex().row() ;
            record(name + "_currentrow", QJsonValue(cur)) ;
            record(name + "_tab", QJsonValue(w.ui->tabBar->currentIndex())) ;
            record(name + "_currenttitle", QJsonValue(cur>=0 ? w.calendarlist.data(w.calendarlist.index(cur,0), Qt::DisplayRole).toString() : QString())) ;
            record(name + "_rows", QJsonValue(w.calendarlist.rowCount())) ;
            {
                QListView *lv = w.ui->listCalendar ;
                QModelIndex ci = lv->currentIndex() ;
                record(name + "_top", QJsonValue(ci.isValid() ? lv->visualRect(ci).top() : -999)) ;
                record(name + "_vh", QJsonValue(lv->viewport()->height())) ;
                record(name + "_bar", QJsonValue(lv->verticalScrollBar()->value())) ;
                record(name + "_mode", QJsonValue((int)lv->verticalScrollMode())) ;
            }

        } else if (kind == "wait") {
            // Lets the event loop run, so that timers fire
            QTest::qWait(op["ms"].toInt()) ;

        } else if (kind == "findnext") {
            // Sets the search text, and runs Find Next (Search > Find Next), from the current row
            w.searchtext = op["text"].toString() ;
            if (op.contains("row")) w.ui->listCalendar->setCurrentIndex(w.calendarlist.index(op["row"].toInt(),0)) ;
            w.activateWindow() ; QApplication::setActiveWindow(&w) ; w.ui->listCalendar->setFocus() ;
            w.on_actionFindNext_triggered() ;
            QModelIndex ci = w.ui->listCalendar->currentIndex() ;
            record(op["name"].toString() + "_row", QJsonValue(ci.row())) ;
            record(op["name"].toString() + "_date", QJsonValue(w.calendarlist.data(w.calendarlist.index(ci.row(),0), Qt::DisplayRole).toString().left(40))) ;

        } else if (kind == "setrow") {
            // Moves the list to a row, with the list focused, as a user has it
            w.activateWindow() ; QApplication::setActiveWindow(&w) ; w.ui->listCalendar->setFocus() ;
            w.ui->listCalendar->setCurrentIndex(w.calendarlist.index(op["row"].toInt(),0)) ;

        } else if (kind == "accinfo") {
            // Reads the text of the screen reader text dialog
            QPlainTextEdit *e = w.accessibleinfo.isNull() ? nullptr : w.accessibleinfo->findChild<QPlainTextEdit*>("AccessibleInfoText") ;
            record(op["name"].toString() + "_text", QJsonValue(e ? e->toPlainText() : QString("NONE"))) ;
            QLabel *note = w.accessibleinfo.isNull() ? nullptr : w.accessibleinfo->findChild<QLabel*>("AccessibleInfoNote") ;
            record(op["name"].toString() + "_note", QJsonValue(note ? note->text() : QString("NONE"))) ;

        } else if (kind == "accdebug") {
            // Sets the accessibility debug checkbox of the screen reader text dialog, and records what it now shows
            QCheckBox *cb = w.accessibleinfo.isNull() ? nullptr : w.accessibleinfo->findChild<QCheckBox*>("AccessibleDebugCheck") ;
            if (cb) cb->setChecked(op["value"].toBool()) ;
            record(op["name"].toString() + "_box", QJsonValue(cb ? cb->isChecked() : false)) ;

        } else if (kind == "measure") {
            // Records where the current row sits in the list, without changing anything
            QString name = op["name"].toString() ;
            QListView *lv = w.ui->listCalendar ;
            QModelIndex ci = lv->currentIndex() ;
            record(name + "_top", QJsonValue(ci.isValid() ? lv->visualRect(ci).top() : -999)) ;
            record(name + "_vh", QJsonValue(lv->viewport()->height())) ;
            record(name + "_row", QJsonValue(ci.row())) ;
            record(name + "_tab", QJsonValue(w.ui->tabBar->currentIndex())) ;

            record(name + "_date", QJsonValue(ci.isValid() ? w.calendarlist.data(w.calendarlist.index(ci.row(),0), Qt::DisplayRole).toString().left(16) : QString())) ;

        } else if (kind == "setting") {
            // Sets an application setting, such as the window (windowbefore) for the next sync
            gConf->settings->setValue(op["key"].toString(), op["value"].toString()) ;
            note(QString("setting %1 = %2").arg(op["key"].toString(), op["value"].toString())) ;

        } else if (kind == "dumplog") {
            // Saves the sync log so far to a file
            QFile f(op["file"].toString()) ;
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) { QTextStream out(&f) ; out << getLog() ; }

        } else if (kind == "listdir") {
            // Lists a folder of the calendar data: its files and sizes
            QString dirpath = gCalendarSavePath + op["sub"].toString() + "/" ;
            QFile f(op["file"].toString()) ;
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f) ;
                out << "folder " << dirpath << "\n" ;
                QFileInfoList items = QDir(dirpath).entryInfoList(QDir::Files, QDir::Name) ;
                for (const QFileInfo &fi : items) out << fi.fileName() << " " << fi.size() << "\n" ;
                out << items.size() << " files\n" ;
            }

        } else if (kind == "paths") {
            QFile f(op["file"].toString()) ;
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) { QTextStream out(&f) ; out << gCalendarSavePath << "\n" ; }

        } else if (kind == "choose") {
            // The letter the next choice dialog is answered with
            pendingChoice = op["letter"].toString() ;

        } else if (kind == "repeat_delete") {
            // Deletes one occurrence of a repeat, as the Delete action does, and any choice is answered by "choose"
            w.LoadCalendarTab() ;
            Appointment &origin = w.calendar.getAppointmentBy(Appointment::Summary, op["summary"].toString()) ;
            if (!origin.isNull()) {
                QDateTime occ = QDateTime::fromString(op["occurrence"].toString(), Qt::ISODate) ;
                QString hint = (occ == origin.getDate(Appointment::From)) ? origin.getField(Appointment::ID)
                                                                          : Calendar::occurrenceHint(origin.getField(Appointment::ID), occ) ;
                QModelIndex index = w.calendarlist.find(hint) ;
                note(QString("repeat_delete hint %1, row %2, rows %3").arg(hint).arg(index.row()).arg(w.calendarlist.rowCount())) ;
                if (!index.isValid()) return ;
                // The list only takes the index from setCurrentIndex when it has focus, which a bench window does not, so
                // the view's own index is set
                w.ui->listCalendar->QListView::setCurrentIndex(index) ;
                note(QString("repeat_delete current row %1, model rows match %2").arg(w.ui->listCalendar->currentIndex().row()).arg(w.ui->listCalendar->model() == &w.calendarlist ? "yes" : "no")) ;
                w.on_actionRemoveAppointment_triggered() ;
            }

        } else if (kind == "repeat_edit") {
            // Saves an edit of a repeat, as the form does: the repeat's own day is moved by shiftdays, and the summary can
            // change.  The focus afterwards is recorded.
            QString name = op["name"].toString() ;
            QDateTime occ = QDateTime::fromString(op["occurrence"].toString(), Qt::ISODate) ;
            Appointment &origin = w.calendar.getAppointmentBy(Appointment::Summary, op["summary"].toString()) ;
            if (!origin.isNull()) {
                QDateTime from = origin.getDate(Appointment::From) ;
                QDateTime to = origin.getDate(Appointment::To) ;
                qint64 shift = op["shiftdays"].toInt() * 86400 + op["shiftminutes"].toInt() * 60 ;
                Appointment result = origin ;
                result.setDate(Appointment::From, occ.addSecs(shift)) ;
                result.setDate(Appointment::To, occ.addSecs(shift + from.secsTo(to))) ;
                if (op.contains("newsummary")) result.setField(Appointment::Summary, op["newsummary"].toString()) ;
                if (op.contains("count")) result.setField(Appointment::RepeatCount, op["count"].toString()) ;
                w.storeAppointmentEdit(result, op["occurrence"].toString()) ;
                // The list keeps the index it is given until it has focus, and a bench window does not get the focus back
                // after a dialog.  So the index it kept is applied, as the list does when the user returns to it.
                if (w.ui->listCalendar->curridx.isValid()) w.ui->listCalendar->QListView::setCurrentIndex(w.ui->listCalendar->curridx) ;
            }
            record(name + "_selected_hint", w.calendar.selectedHint()) ;

            // The row that has the focus after the edit, and how many rows the list has (the last is "Add New")
            int row = w.ui->listCalendar->currentIndex().row() ;
            record(name + "_row", row) ;
            record(name + "_rows", w.calendarlist.rowCount()) ;
            record(name + "_listed", (row >= 0 && row < w.calendarlist.rowCount()) ? w.calendarlist.hintAt(row) : QString()) ;
            note("repeat_edit " + name) ;

        } else if (kind == "form_repeat") {
            // Opens the appointment form with a repeat, and records the repeat it gives back, as a save would store it
            QString name = op["name"].toString() ;
            Appointment appt ;
            appt.createNew() ;
            appt.setField(Appointment::Summary, "BENCH form repeat") ;
            appt.setField(Appointment::Repeat, op["repeat"].toString()) ;
            appt.setField(Appointment::RepeatInterval, op["every"].toString()) ;
            appt.setField(Appointment::RepeatCount, op["count"].toString()) ;
            appt.setField(Appointment::RepeatUntil, op["until"].toString()) ;
            appt.setDate(Appointment::From, QDateTime::currentDateTimeUtc()) ;
            appt.setDate(Appointment::To, QDateTime::currentDateTimeUtc().addSecs(3600)) ;
            Appointment reference = appt ;
            AppointmentForm frm(&w) ;
            frm.setupForm(w.db, QString(), appt, reference, true) ;
            Appointment &back = frm.getAppointmentDetails() ;
            record(name + "_repeat", back.getField(Appointment::Repeat)) ;
            record(name + "_every", back.getField(Appointment::RepeatInterval)) ;
            record(name + "_count", back.getField(Appointment::RepeatCount)) ;
            record(name + "_until", back.getField(Appointment::RepeatUntil)) ;
            note("form_repeat " + name) ;

        } else if (kind == "goto_today") {
            // Goto Today, as the menu runs it; records the row it lands on, and that row's hint
            QString name = op["name"].toString() ;
            w.on_actionGotoToday_triggered() ;
            int row = w.ui->listCalendar->currentIndex().row() ;
            record(name + "_row", row) ;
            record(name + "_hint", (row >= 0 && row < w.calendarlist.rowCount()) ? w.calendarlist.hintAt(row) : QString()) ;
            note("goto_today " + name) ;

        } else if (kind == "calendar_rows") {
            // The calendar list as it is shown: each row's hint and title, in order.  The list is rebuilt first, as the
            // window does when its calendar tab loads, unless "reload" is false: then it is read as it is shown now.
            QString name = op["name"].toString() ;
            if (op["reload"].toBool(true)) w.LoadCalendarTab() ;
            QJsonArray hints, titles ;
            for (int r = 0; r < w.calendarlist.rowCount(); r++) {
                hints.append(w.calendarlist.hintAt(r)) ;
                titles.append(w.calendarlist.data(w.calendarlist.index(r, 0), Qt::DisplayRole).toString()) ;
            }
            record(name + "_hints", hints) ;
            record(name + "_titles", titles) ;

        } else if (kind == "select_overview") {
            // The overview, as selecting it does: no contact is open
            w.populateDialog(SELECT_OVERVIEW, true) ;
            record(op["name"].toString() + "_contact", QJsonValue(w.db.getSelected().getField(Contact::ID))) ;

        } else if (kind == "key") {
            // A key pressed in the calendar list, as a user presses it: the list has the focus, and the key goes to the list
            QString name = op["name"].toString() ;
            QString k = op["key"].toString() ;
            w.activateWindow() ; QApplication::setActiveWindow(&w) ; w.ui->listCalendar->setFocus() ;
            QWidget *target = (op["target"].toString()=="viewport") ? (QWidget*)w.ui->listCalendar->viewport() : (QWidget*)w.ui->listCalendar ;
            if (k=="space") QTest::keyClick(target, Qt::Key_Space) ;
            else if (k=="t") QTest::keyClick(target, Qt::Key_T) ;
            else if (k=="d") QTest::keyClick(target, Qt::Key_D) ;
            else if (k=="n") QTest::keyClick(target, Qt::Key_N) ;
            else if (k=="w") QTest::keyClick(target, Qt::Key_W) ;
            else if (k=="x") QTest::keyClick(target, Qt::Key_X) ;
            else if (k=="p") QTest::keyClick(target, Qt::Key_P) ;
            else if (k=="right") QTest::keyClick(target, Qt::Key_Right) ;
            else if (k=="left") QTest::keyClick(target, Qt::Key_Left) ;
            else if (k.size()==1 && k[0].isDigit()) QTest::keyClick(target, Qt::Key_0 + k[0].digitValue()) ;
            QJsonArray hints, titles ;
            for (int r = 0; r < w.calendarlist.rowCount(); r++) {
                hints.append(w.calendarlist.hintAt(r)) ;
                titles.append(w.calendarlist.data(w.calendarlist.index(r, 0), Qt::DisplayRole).toString()) ;
            }
            record(name + "_hints", hints) ;
            record(name + "_titles", titles) ;
            record(name + "_msg", QJsonValue(w.ui->statusBar->currentMessage())) ;
            record(name + "_contact", QJsonValue(w.db.getSelected().getField(Contact::ID))) ;
            int cur = w.ui->listCalendar->currentIndex().row() ;
            record(name + "_currentrow", QJsonValue(cur)) ;
            record(name + "_currenttitle", QJsonValue(cur>=0 ? w.calendarlist.data(w.calendarlist.index(cur,0), Qt::DisplayRole).toString() : QString())) ;
            record(name + "_focus", QJsonValue(QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : QString())) ;

        } else if (kind == "overview") {
            // The overview of the open contact as it is shown now, with no rebuild
            record(op["name"].toString() + "_overview", QJsonValue(w.ui->editOverview->toPlainText())) ;

        } else if (kind == "edit_ok") {
            // Opens the appointment edit path for an appointment, as Control-U does, on the given occurrence; the form's
            // default button is pressed by the driver.  Records whether the edit path returned.
            QString name = op["name"].toString() ;
            for (const QJsonValue &v : op["froms"].toArray()) {
                nextfroms.append(QDateTime::fromString(v.toString(), Qt::ISODate)) ;
            }
            Appointment &origin = w.calendar.getAppointmentBy(Appointment::Summary, op["summary"].toString()) ;
            if (!origin.isNull()) {
                w.calendar.selectAppointment(op.contains("occurrence") ? Calendar::occurrenceHint(origin.getField(Appointment::ID),
                    QDateTime::fromString(op["occurrence"].toString(), Qt::ISODate)) : origin.getField(Appointment::ID)) ;
                w.editAppointment(origin, origin, QString(), false, op["occurrence"].toString()) ;
            }
            record(name + "_returned", true) ;
            note("edit_ok " + name + " returned") ;

        } else if (kind == "report_clashes") {
            // Runs the check the window does when it opens, on demand
            w.reportClashes() ;
            note("report_clashes done") ;

        } else if (kind == "answer") {
            // Clicks the button with this text on the next dialog that asks a question
            nextanswer = op["text"].toString() ;
            note("answer next question: " + nextanswer) ;

        } else if (kind == "exit") {
            // File > Exit, or Alt-F4: the window's close event decides whether the window closes.  The event is sent
            // without hiding the window, so the remaining operations still run.  Records whether it was accepted.
            QString name = op["name"].toString() ;
            QCloseEvent ev ;
            QApplication::sendEvent(&w, &ev) ;
            record(name + "_exit_accepted", ev.isAccepted()) ;
            note("exit " + name + (ev.isAccepted() ? ": accepted" : ": refused")) ;

        } else if (kind == "form_set") {
            // Types into the form.  With "tidy" set, each entry is tidied as focus leaves it.
            QJsonObject fields = op["fields"].toObject() ;
            QMap<QString, QLineEdit*> entries = formEntries(w.ui) ;
            for (auto it = fields.begin(); it != fields.end(); ++it) {
                if (it.key() == "birthday") { w.ui->editDateOfBirth->setDateTime(xsDateToDateTime(it.value().toString())) ; continue ; }
                if (!entries.contains(it.key())) { note("unknown form field: " + it.key()) ; continue ; }
                entries[it.key()]->setText(it.value().toString()) ;
            }
            if (op["tidy"].toBool()) {
                for (auto it = fields.begin(); it != fields.end(); ++it) {
                    if (entries.contains(it.key())) w.tidyTitledEntry(entries[it.key()]) ;
                }
            }

        } else if (kind == "form_save") {
            // Saves the form as Control-S does, and records whether it was saved and which entry has focus
            QString name = op["name"].toString() ;
            bool saved = w.SaveContactTab() ;
            QWidget *focus = w.focusWidget() ;     // the window's focus, which holds even when it is not the active window
            record(name + "_saved", saved) ;
            record(name + "_focus", focus ? focus->objectName() : QString()) ;
            note(QString("form save %1: %2").arg(name).arg(saved ? "saved" : "refused")) ;

        } else if (kind == "menu_save") {
            // File > Save, then records what the named form entries show, to check that a refused entry keeps its text
            QString name = op["name"].toString() ;
            w.on_actionSave_triggered() ;
            // Whether the details tab is showing, and which entry has focus, after the save
            record(name + "_details", w.ui->tabBar->currentIndex() == CONTACTDETAILSTABPOS) ;
            QWidget *focus = w.focusWidget() ;
            record(name + "_focus", focus ? focus->objectName() : QString()) ;
            QMap<QString, QLineEdit*> entries = formEntries(w.ui) ;
            for (const QJsonValue &f : op["fields"].toArray()) {
                QString field = f.toString() ;
                if (entries.contains(field)) record(name + "_" + field, entries[field]->text()) ;
            }
            note("menu save " + name) ;

        } else if (kind == "clear_selection") {
            // What Tidy Contacts does before it reloads the form: the selection moves to the Overview
            w.db.selectContact(SELECT_OVERVIEW) ;
            w.populateDialog(SELECT_OVERVIEW, true) ;
            note("clear_selection done") ;

        } else if (kind == "merge_duplicates") {
            // The automatic merge of Tidy Contacts: a pair that shares a name and an identifier, with no conflicting
            // details, is merged.  Any other pair is left for the user, and only counted here.
            QString name = op["name"].toString().isEmpty() ? QString("merge") : op["name"].toString() ;
            int merged = 0, left = 0 ;
            for (int i = 0; i < w.db.size(); i++) {
                Contact &first = w.db.getContact(i) ;
                if (first.isSet(Contact::Deleted) || w.db.isUnnamed(first)) continue ;
                for (int j = i + 1; j < w.db.size(); j++) {
                    if (first.isSet(Contact::Deleted)) break ;
                    Contact &second = w.db.getContact(j) ;
                    if (second.isSet(Contact::Deleted) || w.db.isUnnamed(second)) continue ;
                    if (!w.db.isMergeCandidate(first, second)) continue ;
                    Contact mergedcontact ;
                    QList<MergeConflict> conflicts ;
                    w.db.planMerge(first, second, mergedcontact, conflicts) ;
                    if (conflicts.isEmpty() && w.db.isSafeToAutoMerge(first, second)) {
                        w.db.applyMerge(first, second, mergedcontact) ;
                        merged++ ;
                    } else {
                        left++ ;
                    }
                }
            }
            record(name + "_merged", merged) ;
            record(name + "_left", left) ;
            note(QString("%1: %2 merged automatically, %3 left").arg(name).arg(merged).arg(left)) ;

        } else if (kind == "tidy_full") {
            // Tidy Contacts, as the menu runs it.  Records the contact left loaded, and how many live contacts have the
            // surname given, so a scenario can see a merge: the loaded contact must follow it.
            QString name = op["name"].toString() ;
            w.on_actionTidyContacts_triggered() ;
            Contact &loaded = w.db.getSelected() ;
            record(name + "_selected", loaded.getField(Contact::Surname)) ;
            QString surname = op["surname"].toString() ;
            int remaining = 0 ;
            for (int i = 0; i < w.db.size(); i++) {
                Contact &c = w.db.getContact(i) ;
                if (!c.isSet(Contact::Deleted) && c.getField(Contact::Surname) == surname) remaining++ ;
            }
            record(name + "_remaining", remaining) ;
            note(QString("tidy_full %1: %2 loaded, %3 remaining").arg(name).arg(loaded.getField(Contact::Surname)).arg(remaining)) ;

        } else if (kind == "tidy_contacts") {
            // The entry tidy step of Tidy Contacts, run on every contact
            QString name = op["name"].toString().isEmpty() ? QString("tidy") : op["name"].toString() ;
            int contactsChanged = 0 ;
            QStringList unfixed ;
            int changed = w.db.tidyEntries(contactsChanged, unfixed) ;
            if (changed > 0) w.db.save() ;
            QJsonArray lines ;
            for (const QString &line : unfixed) lines.append(line) ;
            record(name + "_changed", changed) ;
            record(name + "_contacts", contactsChanged) ;
            record(name + "_unfixed", lines) ;
            note(QString("%1: %2 entries put right in %3 contacts, %4 not fixed").arg(name).arg(changed).arg(contactsChanged).arg(unfixed.size())) ;

        } else if (kind == "dump") {
            QJsonArray contacts ;
            Contact probe ;
            for (int i = 0; i < w.db.size(); i++) {
                Contact &c = w.db.getContact(i) ;
                QJsonObject obj ;
                for (int r = 0; r < (int)Contact::NumberOfRecords; r++) {
                    QString value = c.getField((Contact::ContactRecord)r) ;
                    if (!value.isEmpty()) obj.insert(QString(probe.contactRecordName((Contact::ContactRecord)r)), value) ;
                }
                contacts.append(obj) ;
            }
            QJsonArray appointments ;
            for (int i = 0; i < w.calendar.size(); i++) {
                Appointment &appt = w.calendar.getAppointment(i) ;
                QJsonObject obj ;
                obj.insert("id", appt.getField(Appointment::ID)) ;
                obj.insert("summary", appt.getField(Appointment::Summary)) ;
                obj.insert("from", appt.getField(Appointment::From)) ;
                obj.insert("to", appt.getField(Appointment::To)) ;
                obj.insert("location", appt.getField(Appointment::Location)) ;
                obj.insert("description", appt.getField(Appointment::Description)) ;
                obj.insert("googlerecordid", appt.getField(Appointment::GoogleRecordId)) ;
                obj.insert("deleted", appt.isSet(Appointment::Deleted)) ;
                obj.insert("readonly", appt.isSet(Appointment::InternetOwned)) ;
                appointments.append(obj) ;
            }
            QJsonObject root ;
            root.insert("contacts", contacts) ;
            root.insert("appointments", appointments) ;
            QString file = outdir + "/" + (op["file"].toString().isEmpty() ? QString("dump.json") : op["file"].toString()) ;
            QFile f(file) ;
            if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(root).toJson()) ;
            note("dumped " + file) ;

        } else {
            note("unknown operation: " + kind) ;
        }

        QTimer::singleShot(0, &a, step) ;
    } ;
    QTimer::singleShot(3000, &a, step) ;

    QTimer::singleShot(240 * 60 * 1000, &a, &QApplication::quit) ;     // safety net

    int result = a.exec() ;
    note("driver exited") ;
    return result ;
}
