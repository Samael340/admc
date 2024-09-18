#include "delegation_permissions_widget.h"

#include "adldap.h"
#include "globals.h"
#include "settings.h"
#include "utils.h"
#include "common_task_manager.h"

#include "samba/ndr_security.h"

#include <QStandardItemModel>
#include <QStandardItem>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
// #include <QComboBox>

class DelegationSortModel final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void set_visible_item_type(DelegationPermissionsWidget::DelegationItemType type) {
        visible_type = type;
        invalidateFilter();
    }

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        const QString name_left = source_left.data(Qt::DisplayRole).toString();
        const QString name_right = source_right.data(Qt::DisplayRole).toString();
        const QString object_type_name_left = source_left.data(RightsItemRole_ObjectTypeName).toString();
        const QString object_type_name_right = source_right.data(RightsItemRole_ObjectTypeName).toString();

        const bool type_not_common_task = visible_type != DelegationPermissionsWidget::DelegationItemType_CommonTask;
        if (type_not_common_task) {
            SecurityRight left = source_left.data(RightsItemRole_SecurityRight).
                    value<SecurityRight>();
            SecurityRight right = source_right.data(RightsItemRole_SecurityRight).
                    value<SecurityRight>();

            const uint32_t access_mask_left = left.access_mask;
            const uint32_t access_mask_right = right.access_mask;
            const bool is_read_left = (access_mask_left == SEC_ADS_READ_PROP);
            const bool is_read_right = (access_mask_right == SEC_ADS_READ_PROP);
            const bool is_create_left = (access_mask_left == SEC_ADS_CREATE_CHILD);
            const bool is_create_right = (access_mask_right == SEC_ADS_DELETE_CHILD);

            // Rights are sorted by object type name
            if (object_type_name_left != object_type_name_right) {
                return object_type_name_left < object_type_name_right;
            }

            // Read rights are before write rights
            if (is_read_left != is_read_right) {
                return is_read_left;
            }

            // Create rights are berfore delete rights
            if (is_create_left != is_create_right) {
                return is_create_right;
            }
        }

        if (object_type_name_left != object_type_name_right) {
            return object_type_name_left < object_type_name_right;
        }

        return name_left < name_right;
    }

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        const QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        bool is_ok = false;
        int item_role = index.data(RightsItemRole_ItemType).toInt(&is_ok);
        if (is_ok && item_role == visible_type) {
            return true;
        }
        return false;
    }

private:
    DelegationPermissionsWidget::DelegationItemType visible_type;
};


DelegationPermissionsWidget::DelegationPermissionsWidget(QWidget *parent) : PermissionsWidget(parent) {
    rights_sort_model = new DelegationSortModel(this);
    rights_sort_model->setSourceModel(rights_model);

    rights_view->setModel(rights_sort_model);
    rights_view->setColumnWidth(PermissionColumn_Name, 400);

    settings_restore_header_state(SETTING_delegation_permissions_header_state, rights_view->header());

    common_delegation_radio_button = new QRadioButton(tr("Common tasks delegation"), this);
    objects_radio_button = new QRadioButton(tr("Create/delete child objects"), this);
    properties_radio_button = new QRadioButton(tr("Read/write properties"), this);


    QHBoxLayout *h_layout = new QHBoxLayout();
    h_layout->addWidget(common_delegation_radio_button);
    h_layout->addWidget(objects_radio_button);
    h_layout->addWidget(properties_radio_button);

    v_layout->addLayout(h_layout);
    v_layout->addWidget(rights_view);
    v_layout->setSizeConstraint(QLayout::SetMaximumSize);

    radio_button_type_hash = {
        {common_delegation_radio_button, DelegationItemType_CommonTask},
        {objects_radio_button, DelegationItemType_ObjectPermission},
        {properties_radio_button, DelegationItemType_PropertyPermission}
    };

    for (QRadioButton *radio_button : radio_button_type_hash.keys()) {
        connect(radio_button, &QRadioButton::clicked, [this, radio_button]() {
               DelegationSortModel *model = static_cast<DelegationSortModel*>(rights_sort_model);
               model->set_visible_item_type(radio_button_type_hash[radio_button]);
        });
    }

    common_delegation_radio_button->setChecked(true);
    common_delegation_radio_button->clicked();

    common_task_manager->init(g_adconfig);
}

DelegationPermissionsWidget::~DelegationPermissionsWidget() {
    settings_save_header_state(SETTING_delegation_permissions_header_state, rights_view->header());
}

void DelegationPermissionsWidget::init(const QStringList &target_classes, security_descriptor *sd_arg) {
    sd = sd_arg;
    target_class_list = target_classes;
    target_class = target_classes.last();

    rights_model->removeRows(0, rights_model->rowCount());

    append_common_tasks();
    append_schema_objects_permissions(DelegationItemType_ObjectPermission);
    append_schema_objects_permissions(DelegationItemType_PropertyPermission);

    rights_sort_model->sort(0);
}

void DelegationPermissionsWidget::append_common_tasks() {
    // Add delegation permissions for corresponding child object classes
    QStringList child_classes = g_adconfig->get_possible_inferiors(target_class);
    QList<CommonTask> child_class_tasks;
    for (const QString &obj_class : common_task_manager->tasks_object_classes()) {
        if (!child_classes.contains(obj_class)) {
            continue;
        }
        child_class_tasks.append(common_task_manager->class_common_task_rights_map[obj_class]);
    }

    for (CommonTask task : child_class_tasks) {
        QString obj_type_name;
        for (const QString &obj_class : common_task_manager->class_common_task_rights_map.keys()) {
            if (common_task_manager->class_common_task_rights_map[obj_class].contains(task)) {
                obj_type_name = obj_class;
            }
        }
        auto row = create_item_row(common_task_manager->common_task_name[task], common_task_manager->common_task_rights[task],
                                   obj_type_name, DelegationItemType_CommonTask);
        rights_model->appendRow(row);
    }
}

QStringList DelegationPermissionsWidget::schema_obj_names(DelegationItemType type) {
    switch (type) {
    case DelegationItemType_ObjectPermission:
        return g_adconfig->get_possible_inferiors(target_class);
    case DelegationItemType_PropertyPermission:
        return g_adconfig->get_permissionable_attributes(target_class);
    default:
        return QStringList();
    }

    return QStringList();
}

QString DelegationPermissionsWidget::permission_text(uint32_t mask, const QString &schema_obj_name)  {
    QString text;

    switch (mask) {
    case SEC_ADS_CREATE_CHILD:
        text = tr("Create ") + g_adconfig->get_class_display_name(schema_obj_name) + tr(" objects");
        break;
    case SEC_ADS_DELETE_CHILD:
        text = tr("Delete ") + g_adconfig->get_class_display_name(schema_obj_name) + tr(" objects");
        break;
    case SEC_ADS_WRITE_PROP:
        text = tr("Write ") + g_adconfig->get_column_display_name(schema_obj_name) + tr(" property");
        break;
    case SEC_ADS_READ_PROP:
        text = tr("Read ") + g_adconfig->get_column_display_name(schema_obj_name) + tr(" property");
        break;
    }

    return text;
}

QList<SecurityRight> DelegationPermissionsWidget::schemas_rights(DelegationItemType type, AdConfig *ad_config, const QString &schema_obj_name) {
    QList<SecurityRight> rights;

    switch (type) {
    case DelegationItemType_ObjectPermission:
        rights = creation_deletion_rights_for_class(ad_config, schema_obj_name);
        return rights;
    case DelegationItemType_PropertyPermission:
        rights = read_write_property_rights(ad_config, schema_obj_name);
        return rights;
    default:
        return rights;
    }

    return rights;
}

void DelegationPermissionsWidget::append_schema_objects_permissions(DelegationItemType type) {
    const QStringList schema_names = schema_obj_names(type);
    for (const QString &schema_obj_name : schema_names) {
        const QList<SecurityRight> rights = schemas_rights(type, g_adconfig, schema_obj_name);
        if (rights.isEmpty()) {
            continue;
        }

        for (const SecurityRight right :rights) {
            const QString name = permission_text(right.access_mask, schema_obj_name);
            auto row = create_item_row(name, {right}, schema_obj_name, type);
            rights_model->appendRow(row);
        }
    }
}

QList<QStandardItem *> DelegationPermissionsWidget::create_item_row(const QString &name, const QList<SecurityRight> rights, const QString &object_type_name, DelegationItemType type) {
    const QList<QStandardItem *> row = make_item_row(PermissionColumn_COUNT);

    row[PermissionColumn_Name]->setText(name);
    row[PermissionColumn_Name]->setEditable(false);
    row[PermissionColumn_Allowed]->setCheckable(true);
    row[PermissionColumn_Allowed]->setEditable(false);
    row[PermissionColumn_Denied]->setCheckable(true);
    row[PermissionColumn_Denied]->setEditable(false);

    row[0]->setData(int(type), RightsItemRole_ItemType);
    row[0]->setData(object_type_name, RightsItemRole_ObjectTypeName);

    if (type == DelegationItemType_CommonTask) {
        QVariant right_data;
        right_data.setValue(rights);
        row[0]->setData(right_data, RightsItemRole_SecurityRightList);
        return row;
    }

    QVariant right_data;
    right_data.setValue(rights.first());
    row[0]->setData(right_data, RightsItemRole_SecurityRight);
    return row;
}
