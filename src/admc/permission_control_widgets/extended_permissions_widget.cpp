#include "extended_permissions_widget.h"

#include "adldap.h"
#include "globals.h"
#include "settings.h"
#include "utils.h"

#include "samba/ndr_security.h"

#include <QStandardItemModel>
#include <QStandardItem>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QDebug>


class ExtendedRightsSortModel final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        const ExtendedPermissionsWidget::ExtRightsItemType item_type_left = (ExtendedPermissionsWidget::ExtRightsItemType)source_left.
                data(RightsItemRole_ItemType).toInt();
        const ExtendedPermissionsWidget::ExtRightsItemType item_type_right = (ExtendedPermissionsWidget::ExtRightsItemType)source_right.
                data(RightsItemRole_ItemType).toInt();
        const QString name_left = source_left.data(Qt::DisplayRole).toString();
        const QString name_right = source_right.data(Qt::DisplayRole).toString();
        const QString object_type_name_left = source_left.data(RightsItemRole_ObjectTypeName).toString();
        const QString object_type_name_right = source_right.data(RightsItemRole_ObjectTypeName).toString();
        const QByteArray object_type_left = source_left.data(RightsItemRole_ObjectType).toByteArray();
        const QByteArray object_type_right = source_right.data(RightsItemRole_ObjectType).toByteArray();
        const uint32_t access_mask_left = source_left.data(RightsItemRole_AccessMask).toUInt();
        const uint32_t access_mask_right = source_right.data(RightsItemRole_AccessMask).toUInt();
        const bool is_control_left = (access_mask_left == SEC_ADS_CONTROL_ACCESS);
        const bool is_control_right = (access_mask_right == SEC_ADS_CONTROL_ACCESS);
        const bool is_read_left = (access_mask_left == SEC_ADS_READ_PROP);
        const bool is_read_right = (access_mask_right == SEC_ADS_READ_PROP);

        // "This object permissions" top item is before
        // "Inherited object permissions" top item
        if (item_type_left != item_type_right) {
            return item_type_left == ExtendedPermissionsWidget::ExtRightsItemType_CurrentObjTitile;
        }

        // Control rights are before read/write rights
        if (is_control_left != is_control_right) {
            return is_control_left;
        }

        // Control rights are sorted by name
        if (is_control_left && is_control_right) {
            return name_left < name_right;
        }

        // Read/write rights are sorted by name
        if (object_type_left != object_type_right) {
            return object_type_name_left < object_type_name_right;
        }

        // Read rights are before write rights
        if (is_read_left != is_read_right) {
            return is_read_left;
        }

        return name_left < name_right;
    }
};

ExtendedPermissionsWidget::ExtendedPermissionsWidget(QWidget *parent) : PermissionsWidget(parent) {
    rights_sort_model = new ExtendedRightsSortModel(this);
    rights_sort_model->setSourceModel(rights_model);

    rights_view->setModel(rights_sort_model);
    rights_view->setColumnWidth(AceColumn_Name, 400);

    settings_restore_header_state(SETTING_extended_permissions_header_state, rights_view->header());
}

ExtendedPermissionsWidget::~ExtendedPermissionsWidget() {
    settings_save_header_state(SETTING_extended_permissions_header_state, rights_view->header());
}

void ExtendedPermissionsWidget::init(const QStringList &target_classes, security_descriptor *sd_arg) {
    sd = sd_arg;
    target_class_list = target_classes;
    rights_model->removeRows(0, rights_model->rowCount());

    QList<QStandardItem *> current_obj_row = create_title_row(tr("Current object permissions"), ExtRightsItemType_CurrentObjTitile);
    const QList<SecurityRight> object_right_list = ad_security_get_extended_rights_for_class(g_adconfig, target_class_list);
    append_child_right_items(current_obj_row[0], object_right_list, tr("There are no extended rights for this class of objects"));

    QList<QStandardItem *> inherited_objs_row = create_title_row(tr("Child object permissions"), ExtRightsItemType_InheritedObjsTitle);
    const QString obj_class = target_class_list.last();
    QStringList inferior_classes = g_adconfig->get_possible_inferiors(obj_class);

    rights_model->appendRow(current_obj_row);
    rights_model->appendRow(inherited_objs_row);

    // Remove object's classes because corresponding rights have been already received
    // and appened to object's extended rights
    for(const QString &class_to_remove : target_class_list) {
        if (inferior_classes.contains(class_to_remove)) {
            inferior_classes.removeAll(class_to_remove);
        }
    }

    QList<SecurityRight> inferior_right_list;
    for(const QString &inferior : inferior_classes) {
        QList<SecurityRight> appended_right_list = ad_security_get_extended_rights_for_class(g_adconfig, {inferior});
        // Remove duplicated extended rights
        for (const SecurityRight &right : appended_right_list) {
            if (inferior_right_list.contains(right)) {
                continue;
            }
            inferior_right_list.append(right);
        }
    }
    append_child_right_items(inherited_objs_row[0], inferior_right_list, tr("There are no extended rights for inferior classes of the given object"));

    if (read_only) {
        make_model_rights_read_only(QModelIndex());
    }

    rights_sort_model->sort(0);
}

void ExtendedPermissionsWidget::update_model_rights(const QModelIndex &parent) {
    Q_UNUSED(parent)
    for (int row = 0; row < rights_model->rowCount(); row++) {
        const QModelIndex title_index = rights_model->index(row, 0);
        PermissionsWidget::update_model_rights(title_index);
    }

    rights_view->expandAll();
}

void ExtendedPermissionsWidget::make_model_rights_read_only(const QModelIndex &parent) {
    Q_UNUSED(parent)
    for (int row = 0; row < rights_model->rowCount(); row++) {
        const QModelIndex title_index = rights_model->index(row, 0);
        PermissionsWidget::make_model_rights_read_only(title_index);
    }
}

QList<QStandardItem *> ExtendedPermissionsWidget::make_right_item_row(const SecurityRight &right, uint8_t flags) {
    const QList<QStandardItem *> row = make_item_row(AceColumn_COUNT);

    const QString right_name = ad_security_get_right_name(g_adconfig, right, language);

    const QString object_type_name = g_adconfig->get_right_name(right.object_type, language);

    row[AceColumn_Name]->setText(right_name);
    row[AceColumn_Allowed]->setCheckable(true);
    row[AceColumn_Denied]->setCheckable(true);

    row[0]->setData(right.access_mask, RightsItemRole_AccessMask);
    row[0]->setData(right.object_type, RightsItemRole_ObjectType);
    row[0]->setData(object_type_name, RightsItemRole_ObjectTypeName);
    row[0]->setData(ExtRightsItemType_ExtRight, RightsItemRole_ObjectTypeName);
    row[0]->setData(int(flags), RightsItemRole_ACE_Flags);
    row[0]->setData(right.inherited_object_type, RightsItemRole_InheritedObjectType);
    row[0]->setEditable(false);

    return row;
}

QList<QStandardItem *> ExtendedPermissionsWidget::create_title_row(const QString &text, ExtRightsItemType type) {
    const QList<QStandardItem *> row = make_item_row(AceColumn_COUNT);
    for (QStandardItem *title_item : row) {
        title_item->setEditable(false);
    }

    QFont font;
    font.setBold(true);
    QStandardItem *main_item = row[0];
    main_item->setFont(font);
    main_item->setData(type, RightsItemRole_ItemType);
    main_item->setText(text);

    return row;
}

void ExtendedPermissionsWidget::append_child_right_items(QStandardItem *title_item, QList<SecurityRight> rights, const QString &default_item_text) {
    if (rights.isEmpty()) {
        title_item->appendRow(new QStandardItem(default_item_text));
        return;
    }

    uint8_t flags = 0;
    ExtRightsItemType parent_item_type = (ExtRightsItemType)title_item->data(RightsItemRole_ItemType).toInt();
    if (parent_item_type == ExtRightsItemType_InheritedObjsTitle) {
        flags = SEC_ACE_FLAG_INHERIT_ONLY /*| SEC_ACE_FLAG_NO_PROPAGATE_INHERIT*/;
    }

    for(const SecurityRight &right : rights) {
       QList<QStandardItem*> right_row =  make_right_item_row(right, flags);
       title_item->appendRow(right_row);
    }
}

void ExtendedPermissionsWidget::append_combined_permissions() {

}


