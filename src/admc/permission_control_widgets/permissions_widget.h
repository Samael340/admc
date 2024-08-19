#ifndef PERMISSIONS_WIDGET_H
#define PERMISSIONS_WIDGET_H

#include <QWidget>
#include <QLocale>

class QStandardItemModel;
class QStandardItem;
struct security_descriptor;
class QTreeView;
class AdInterface;
class QSortFilterProxyModel;
class QModelIndex;

enum RightsItemRole {
    RightsItemRole_AccessMask = Qt::UserRole,
    RightsItemRole_ObjectType,
    RightsItemRole_ObjectTypeName,
    RightsItemRole_ItemType,
    RightsItemRole_ACE_Flags,
    RightsItemRole_InheritedObjectType
};

class PermissionsWidget : public QWidget {
    Q_OBJECT

public:
    explicit PermissionsWidget(QWidget *parent = nullptr);
    virtual ~PermissionsWidget() = default;

    virtual void init(const QStringList &target_classes,
                      security_descriptor *sd) = 0;
    virtual void set_read_only();
    void set_current_trustee(const QByteArray &current_trustee);

signals:
    void edited();

protected:
    enum AceColumn {
        AceColumn_Name,
        AceColumn_Allowed,
        AceColumn_Denied,

        AceColumn_COUNT,
    };

    bool ignore_item_changed_signal;
    security_descriptor *sd;
    bool read_only;
    QStandardItemModel *rights_model;
    QTreeView *rights_view;
    QByteArray trustee;
    QStringList target_class_list;
    QLocale::Language language;
    QSortFilterProxyModel *rights_sort_model;

    virtual void on_item_changed(QStandardItem *item);
    virtual void update_model_rights(const QModelIndex &parent);

    virtual void make_model_rights_read_only(const QModelIndex &parent);
};

#endif // PERMISSIONS_WIDGET_H
