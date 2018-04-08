#ifndef LOGFORM_H
#define LOGFORM_H

#include <QDialog>

namespace Ui {
class LogForm;
}

class LogForm : public QDialog
{
    Q_OBJECT

public:
    explicit LogForm(QWidget *parent = 0);
    ~LogForm();
    void setText(QString& text) ;

private:
    Ui::LogForm *ui;
};

#endif // LOGFORM_H
