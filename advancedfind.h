#ifndef ADVANCEDFIND_H
#define ADVANCEDFIND_H

#include <QDialog>
#include "contact.h"

namespace Ui {
class AdvancedFind;
}

class AdvancedFind : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedFind(QWidget *parent = 0);
    ~AdvancedFind();

    Contact::ContactRecord categorySearch1() ;
    Contact::ContactRecord categorySearch2() ;
    Contact::ContactRecord categoryFlag1() ;
    Contact::ContactRecord categoryFlag2() ;
    QString searchText1() ;
    QString searchText2() ;
    bool flagChecked1() ;
    bool flagChecked2() ;

private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

    void on_comboBox_SearchCategory1_currentIndexChanged(int index);

    void on_comboBox_SearchCategory2_currentIndexChanged(int index);

    void on_comboBox_SearchFlag1_currentIndexChanged(int index);

    void on_comboBox_SearchFlag2_currentIndexChanged(int index);

private:
    Ui::AdvancedFind *ui;
};

#endif // ADVANCEDFIND_H
