#ifndef AccessibleStringListModel_H
#define AccessibleStringListModel_H

#include <QVariant>
#include <QModelIndex>
#include <QStringListModel>
#include <QList>
#include "accessiblestringlistmodel.h"

//
// Creating custom QStringListModel
//
// https://het.as.utexas.edu/HET/Software/html/qabstractitemmodel.html#data
//

class AccessibleStringListModel : public QStringListModel
{
private:
    QList<QList<QVariant>> modeldata ;

signals:
 //   void dataChanged(const QModelIndex &topleft, const QModelIndex &bottomright) const;

public:
    AccessibleStringListModel();

    // Overridden functions
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    bool setData( const QModelIndex &index, QVariant data, int role = Qt::DisplayRole) ;
    bool insertRows(int row, int column, const QModelIndex &parent = QModelIndex()) ;

    void clear() ;
    bool setData(const QModelIndex &index, QVariant display, QVariant accessibletext, QVariant user) ;
    bool appendData(QVariant display, QVariant accessibletext, QVariant user) ;

    QString hintAt(int row) ;
    QModelIndex find(QVariant user, int startrow=0);

    void debug() ;

};

#endif // AccessibleStringListModel_H
