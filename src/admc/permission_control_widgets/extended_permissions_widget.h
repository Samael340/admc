#ifndef EXTENDEDPERMISSIONSWIDGET_H
#define EXTENDEDPERMISSIONSWIDGET_H

#include "permission_control_widgets/permissions_widget.h"

struct SecurityRight;

class ExtendedPermissionsWidget final : public PermissionsWidget {
    Q_OBJECT

public:
    enum ExtRightsItemType {
        ExtRightsItemType_CurrentObjTitile,
        ExtRightsItemType_InheritedObjsTitle,
        ExtRightsItemType_ExtRight,

        // Combined permissions are permissions that consist of
        // several permissions like "User creation, deletion and control".
        // These permissions are not found among extended rights.
        ExtRightsItemType_CombinedPermission
    };

    enum CombinedPermission {
        CombinedPermission_UserControl, // User
    };

    ExtendedPermissionsWidget(QWidget *parent = nullptr);
    ~ExtendedPermissionsWidget();

    virtual void init(const QStringList &target_classes,
                      security_descriptor *sd_arg) override;

private:
    virtual void update_model_rights(const QModelIndex &parent) override;
    virtual void make_model_rights_read_only(const QModelIndex &parent) override;

    QList<QStandardItem*> make_right_item_row(const SecurityRight &right, uint8_t flags);
    QList<QStandardItem*> create_title_row(const QString &text, ExtRightsItemType type);
    void append_child_right_items(QStandardItem *title_item, QList<SecurityRight> rights, const QString &default_item_text);
    void append_combined_permissions();

    QHash<QString, QList<SecurityRight>> combined_rights;
};

#endif // EXTENDEDPERMISSIONSWIDGET_H
