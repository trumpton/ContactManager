// Loads a contact file written before the email and phone labels existed, and checks that it keeps its meaning.
// The working folder (argv[1]) must hold old-contact.contact and old-other.contact; the test writes its copy there too.
// Build: see autotest/HEADLESS-TEST.md, section 5a.  Run with CM_TEST_SECRET set, as the bench does.
#include <QApplication>
#include <QFile>
#include <QDir>
#include <cstdio>
#include "configuration.h"
#include "../../../Lib/encryption.h"
#include "contact.h"
#include "../../../Lib/alertsound.h"
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QtGlobal>
#include <QMessageBox>

extern Configuration *gConf ;
static int fails = 0 ;
static void check(bool ok, const char *what, QString detail = QString()) {
    printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", what, detail.isEmpty() ? "" : " - ", qPrintable(detail)) ;
    if (!ok) fails++ ;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv) ;
    QString dir = QString::fromLocal8Bit(argv[1]) + "/" ;

    initSound(NULL) ;

    // Answer the key dialog with the test secret, as the bench driver does
    QTimer *tick = new QTimer(&app) ;
    QObject::connect(tick, &QTimer::timeout, [&]() {
        QWidget *m = QApplication::activeModalWidget() ;
        if (!m || QString(m->metaObject()->className()) != "Encryption") return ;
        QString secret = QString::fromLocal8Bit(qgetenv("CM_TEST_SECRET")) ;
        QStringList fields = (m->windowTitle() == "Login") ? QStringList() << "lineEdit2"
                                                            : QStringList() << "lineEdit0" << "lineEdit1" << "lineEdit2" << "lineEdit3" ;
        for (const QString &name : fields) {
            if (auto *e = m->findChild<QLineEdit*>(name)) e->setText(secret) ;
        }
        if (auto *box = m->findChild<QDialogButtonBox*>()) box->button(QDialogButtonBox::Ok)->click() ;
    }) ;
    tick->start(200) ;
    gConf = new Configuration(nullptr) ;
    Encryption *enc = new Encryption(QString("trumpton.uk"), QString("TextFileEncryption")) ;
    gConf->setEncryption(enc) ;

    Contact c ;
    check(c.load(dir, "old-contact"), "an old file loads") ;

    check(c.getField(Contact::Email) == "sam@sgc.com" && c.getField(Contact::Email2) == "sam2@scg.com"
          && c.getField(Contact::Email3) == "sam3@sgc.com", "the three addresses are in their slots") ;
    check(c.emailLabel(0) == "Home" && c.emailLabel(1) == "Other" && c.emailLabel(2) == "Other" && c.emailLabel(3) == "Other",
          "email labels are the old meanings", QString("%1 %2 %3 %4").arg(c.emailLabel(0), c.emailLabel(1), c.emailLabel(2), c.emailLabel(3))) ;
    check(c.phoneLabel(0) == "Home" && c.phoneLabel(1) == "Work" && c.phoneLabel(2) == "Mobile",
          "phone labels Home, Work, Mobile") ;
    check(c.getField(Contact::Phone2) == "+44 20 7946 0002" && c.phoneLabel(3) == "Pager",
          "a phone2 title in an old file is kept as its label", QString(c.phoneLabel(3))) ;
    Contact o ;
    check(o.load(dir, "old-other") && o.getField(Contact::Phone2) == "+44 20 7946 0003",
          "an otherphone line from an older version is read as phone 2, as it always was", o.getField(Contact::Phone2)) ;
    check(c.getField(Contact::Mobile) == "+44 7700 900123", "mobile is read") ;
    printf("values: phone=[%s] phone2=[%s] phone3=[%s] phone2title=[%s]\n", qPrintable(c.getField(Contact::Phone)),
           qPrintable(c.getField(Contact::Phone2)), qPrintable(c.getField(Contact::Phone3)), qPrintable(c.getField(Contact::Phone2Title))) ;

    // Save it and load it again: the labels and addresses come back unchanged
    c.markAsDirty() ;   // a loaded contact is not dirty, so save writes nothing unless it is marked
    check(c.save(dir), "the old contact saves") ;
    Contact d ;
    check(d.load(dir, c.getField(Contact::ID)), "the saved copy loads") ;
    check(d.getField(Contact::Email) == "sam@sgc.com" && d.getField(Contact::Email2) == "sam2@scg.com"
          && d.getField(Contact::Email3) == "sam3@sgc.com", "addresses are unchanged after a save") ;
    check(d.emailLabel(0) == "Home" && d.emailLabel(1) == "Other" && d.emailLabel(2) == "Other" && d.emailLabel(3) == "Other",
          "labels are unchanged after a save") ;
    check(d.phoneLabel(3) == "Pager", "phone title is unchanged after a save") ;

    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails) ;
    return fails ? 1 : 0 ;
}
