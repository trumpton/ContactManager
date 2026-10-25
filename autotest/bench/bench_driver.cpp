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
// usage: bench_driver <outdir> <operations.json>
//
#include <QApplication>
#include <QTimer>
#include <QAction>
#include <QDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
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
#include <functional>
#include "../Lib/alertsound.h"
#define private public
#include "mainwindow.h"
#undef private
#include "contact.h"
#include "contactdatabase.h"
#include "calendar.h"
#include "appointment.h"

static QString outdir ;
static int syncCount = 0 ;

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
    return -1 ;
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
    QTimer *tick = new QTimer(&a) ;
    QObject::connect(tick, &QTimer::timeout, [&]() {
        QWidget *m = QApplication::activeModalWidget() ;
        if (!m) return ;

        if (m == lastmodal) {
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
            QTimer::singleShot(500, &a, &QApplication::quit) ;
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

    QTimer::singleShot(20 * 60 * 1000, &a, &QApplication::quit) ;     // safety net

    int result = a.exec() ;
    note("driver exited") ;
    return result ;
}
