#include "mergeconfirmdialog.h"
#include "../Lib/alertsound.h"

#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QEvent>

MergeConfirmDialog::MergeConfirmDialog(QWidget *parent, QString title, QString message) :
    QDialog(parent)
{
    setWindowTitle(title) ;
    setModal(true) ;
    setMinimumWidth(480) ;

    details = new QTextEdit(this) ;
    details->setReadOnly(true) ;
    details->setPlainText(message) ;
    details->setAccessibleName("Possible duplicate details") ;
    details->installEventFilter(this) ;

    mergebutton = new QPushButton("&Merge", this) ;
    mergebutton->setAccessibleName("Merge") ;
    mergebutton->installEventFilter(this) ;
    connect(mergebutton, &QPushButton::clicked, this, &QDialog::accept) ;

    skipbutton = new QPushButton("&Skip", this) ;
    skipbutton->setAccessibleName("Skip") ;
    skipbutton->setDefault(true) ;
    skipbutton->installEventFilter(this) ;
    connect(skipbutton, &QPushButton::clicked, this, &QDialog::reject) ;

    QHBoxLayout *buttons = new QHBoxLayout ;
    buttons->addStretch() ;
    buttons->addWidget(mergebutton) ;
    buttons->addWidget(skipbutton) ;

    QVBoxLayout *layout = new QVBoxLayout(this) ;
    layout->addWidget(details) ;
    layout->addLayout(buttons) ;

    // Tab goes from the details, to Merge, and then to Skip
    setTabOrder(details, mergebutton) ;
    setTabOrder(mergebutton, skipbutton) ;
    details->setFocus() ;

    play(Query) ;
}

bool MergeConfirmDialog::eventFilter(QObject *watched, QEvent *event)
{
    // Enter skips, from any of the controls in the dialog
    if (event->type()==QEvent::KeyPress) {
        QKeyEvent *key = static_cast<QKeyEvent *>(event) ;
        if (key->key()==Qt::Key_Enter || key->key()==Qt::Key_Return) {
            reject() ;
            return true ;
        }
    }
    return QDialog::eventFilter(watched, event) ;
}
