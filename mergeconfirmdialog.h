#ifndef MERGECONFIRMDIALOG_H
#define MERGECONFIRMDIALOG_H

#include <QDialog>
#include <QString>

class QTextEdit ;
class QPushButton ;

// Asks whether two contacts should be merged.  The details are in a read-only text area, which
// has the focus, so that a screen reader reads them.  Tab moves to Merge, and Tab again to Skip.
// Enter always skips, whichever control has the focus, so an accidental Enter cannot merge.
// Escape also skips.
class MergeConfirmDialog : public QDialog
{
public:
    MergeConfirmDialog(QWidget *parent, QString title, QString message) ;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override ;

private:
    QTextEdit *details ;
    QPushButton *mergebutton ;
    QPushButton *skipbutton ;
} ;

#endif // MERGECONFIRMDIALOG_H
