#include "accessiblestringlistmodel.h"
#include <QRegExp>

AccessibleStringListModel::AccessibleStringListModel()
{

}

void AccessibleStringListModel::clear()
{
    modeldata.clear() ;
}


bool AccessibleStringListModel::appendData(QVariant display, QVariant accessibletext, QVariant user)
{
    if (insertRows(rowCount(), 1)) {
        return setData(index(rowCount()-1, 0), display, accessibletext, user) ;
    } else {
        return false ;
    }
}

bool AccessibleStringListModel::setData(const QModelIndex &index, QVariant display, QVariant accessibletext, QVariant user)
{
    if (!setData(index, display, Qt::DisplayRole)) return false ;
    if (!setData(index, accessibletext, Qt::AccessibleTextRole)) return false ;
    if (!setData(index, user, Qt::UserRole)) return false ;
    return true ;
}


bool AccessibleStringListModel::insertRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent) ;
    QList<QVariant> newentry ;
    newentry.append(QVariant(""));
    newentry.append(QVariant(""));
    newentry.append(QVariant(""));
    beginInsertRows(parent, row, row + count - 1) ;
    for (int i=0; i<count; i++) {
        modeldata.insert(row, newentry) ;
    }
    endInsertRows();
    return true ;
}

bool AccessibleStringListModel::setData(const QModelIndex &index, QVariant data, int role)
{
    int row = index.row() ;
    if (row<0 || row>=modeldata.size()) return false ;

    const QList<QVariant>& selected = modeldata.at(row) ;
    switch (role) {

    case Qt::DisplayRole: {
            QVariant& item = (QVariant&)selected.at(0) ;
            item.setValue(data) ;
            emit dataChanged(index, index) ;
            return true ;
        }
        break ;

    case Qt::AccessibleTextRole: {
            QVariant& item = (QVariant&)selected.at(1) ;
            item.setValue(data) ;
            emit dataChanged(index, index) ;
            return true ;
        }
        break ;

    case Qt::UserRole: {
            QVariant& item = (QVariant&)selected.at(2) ;
            item.setValue(data) ;
            emit dataChanged(index, index) ;
            return true ;
        }
        break ;

    default:
        return false ;
        break ;

    }
}

QVariant AccessibleStringListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row() ;
    if (row<0 || row>=modeldata.size()) return QVariant() ;
    const QList<QVariant>& selected = modeldata.at(row) ;
    switch (role){
        case Qt::DisplayRole:
            return selected.at(0) ;
            break ;
        case Qt::AccessibleTextRole:
            return selected.at(1) ;
            break ;
        case Qt::UserRole:
            return selected.at(2) ;
            break ;
        default: return QVariant() ;
    }
}


QModelIndex AccessibleStringListModel::find(QVariant user, int startrow)
{
    int result=-1 ;
    QRegExp re(QString(".*%1.*").arg(user.toString().toLower())) ;
    for (int i=startrow; result<0 && i<modeldata.size(); i++) {
        QString srch = modeldata.at(i).toVector().at(2).toString().toLower() ;
        if (re.indexIn(srch)) result=i ;
    }
    if (result<0) return QModelIndex() ;
    else return index(result,0) ;
}

QString AccessibleStringListModel::hintAt(int row)
{
    const QVariant& result = data(index(row,0), Qt::UserRole) ;
    if (!result.isValid()) return QString("") ;
    else return result.toString() ;
}

QModelIndex AccessibleStringListModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent) ;
    Q_UNUSED(column) ;
    return createIndex(row, column) ;
}

int AccessibleStringListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent) ;
    return modeldata.size() ;
}

Qt::ItemFlags AccessibleStringListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void AccessibleStringListModel::debug()
{

}
