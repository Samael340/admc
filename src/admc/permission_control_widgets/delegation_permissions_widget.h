#ifndef DELEGATION_PERMISSIONS_WIDGET_H
#define DELEGATION_PERMISSIONS_WIDGET_H

#include "permissions_widget.h"

struct SecurityRight;
class QRadioButton;
class AdConfig;

class DelegationPermissionsWidget final : public PermissionsWidget {

    Q_OBJECT

public:
    enum DelegationItemType {
        DelegationItemType_ObjectPermission,
        DelegationItemType_PropertyPermission,
        DelegationItemType_CommonTask
    };

    DelegationPermissionsWidget(QWidget *parent = nullptr);
    ~DelegationPermissionsWidget();

    virtual void init(const QStringList &target_classes,
                      security_descriptor *sd_arg) override;
private:
    void append_common_tasks();
    void append_schema_objects_permissions(DelegationItemType type);

    QList<QStandardItem*> create_item_row(const QString &name, const QList<SecurityRight> rights, const QString &object_type_name,
                         DelegationItemType type);

    QRadioButton *common_delegation_radio_button;
    QRadioButton *objects_radio_button;
    QRadioButton *properties_radio_button;
    QHash<QRadioButton*, DelegationItemType> radio_button_type_hash;

    QString target_class;

    QStringList schema_obj_names(DelegationItemType type);
    QString permission_text(uint32_t mask, const QString &schema_obj_name);
    QList<SecurityRight> schemas_rights(DelegationItemType type, AdConfig *ad_config, const QString &schema_obj_name);
};

#endif // DELEGATION_PERMISSIONS_WIDGET_H
