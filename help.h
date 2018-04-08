#ifndef HELP_H
#define HELP_H

#include <QDialog>
#include <QString>

namespace Ui {
class Help;
}

class Help : public QDialog
{
    Q_OBJECT

public:
    explicit Help(QWidget *parent = 0);
    ~Help();

private:
    void keyPressEvent(QKeyEvent *event) ;
    void findFirst();
    void findNext();

    QString searchtext ;
    Ui::Help *ui;
};

#endif // HELP_H
