/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2024 BaseALT Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    rights_view->setColumnWidth(PermissionColumn_Name, 400);

    load_common_tasks();

    settings_restore_header_state(SETTING_extended_permissions_header_state, rights_view->header());
}

ExtendedPermissionsWidget::~ExtendedPermissionsWidget() {
    settings_save_header_state(SETTING_extended_permissions_header_state, rights_view->header());
}

void ExtendedPermissionsWidget::init(const QStringList &target_classes, security_descriptor *sd_arg) {
    sd = sd_arg;
    target_class_list = target_classes;
    rights_model->removeRows(0, rights_model->rowCount());


    append_extended_right_items();
    append_common_task_items();

    if (read_only) {
        make_model_rights_read_only(QModelIndex());
    }

    rights_sort_model->sort(0);
}

void ExtendedPermissionsWidget::update_model_right_items(const QModelIndex &parent) {
    Q_UNUSED(parent)
    for (int row = 0; row < rights_model->rowCount(); row++) {
        const QModelIndex title_index = rights_model->index(row, 0);
        PermissionsWidget::update_model_right_items(title_index);
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

void ExtendedPermissionsWidget::on_item_changed(QStandardItem *item) {

}

QList<QStandardItem *> ExtendedPermissionsWidget::make_right_item_row(const QString &name, const QList<SecurityRight> &rights) {
    const QList<QStandardItem *> row = make_item_row(PermissionColumn_COUNT);

    const QString right_name = ad_security_get_right_name(g_adconfig, rights, language);

    const QString object_type_name = g_adconfig->get_right_name(rights.object_type, language);

    row[PermissionColumn_Name]->setText(name);
    row[PermissionColumn_Allowed]->setCheckable(true);
    row[PermissionColumn_Denied]->setCheckable(true);

    row[0]->setData(rights.access_mask, RightsItemRole_AccessMask);
    row[0]->setData(rights.object_type, RightsItemRole_ObjectType);
    row[0]->setData(object_type_name, RightsItemRole_ObjectTypeName);
    row[0]->setData(ExtRightsItemType_ExtRight, RightsItemRole_ItemType);
    row[0]->setData(uint(flags), RightsItemRole_ACE_Flags);
    row[0]->setData(rights.inherited_object_type, RightsItemRole_InheritedObjectType);
    row[0]->setEditable(false);

    return row;
}

QList<QStandardItem *> ExtendedPermissionsWidget::create_title_row(const QString &text, ExtRightsItemType type) {
    const QList<QStandardItem *> row = make_item_row(PermissionColumn_COUNT);
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

void ExtendedPermissionsWidget::append_extended_right_items() {
    const QList<SecurityRight> extended_right_list = ad_security_get_extended_rights_for_class(g_adconfig, target_class_list);

    for(const SecurityRight &right : extended_right_list) {
       QList<QStandardItem*> right_row = make_right_item_row(right, flags);
       title_item->appendRow(right_row);
    }
}

void ExtendedPermissionsWidget::load_common_tasks() {
    class_common_task_rights_map[CLASS_USER] = {CommonTask_UserControl,
                                           CommonTask_ChangeUserPassword,
                                           CommonTask_ReadAllUserInformation};
    class_common_task_rights_map[CLASS_GROUP] = {CommonTask_GroupControl};
    class_common_task_rights_map[CLASS_INET_ORG_PERSON] = {CommonTask_InetOrgPersonControl,
                                                      CommonTask_ChangeInetOrgPersonPassword,
                                                      CommonTask_ReadAllInetOrgPersonInformation};

    common_task_name[CommonTask_UserControl] = tr("Create, delete and manage user accounts");
    common_task_name[CommonTask_ChangeUserPassword] = tr("Reset user passwords and force password change at next logon");
    common_task_name[CommonTask_ReadAllUserInformation] = tr("Read all user information");
    common_task_name[CommonTask_GroupControl] = tr("Create, delete and manage groups");
    common_task_name[CommonTask_GroupMembership] = tr("Modify the membership of group");
    common_task_name[CommonTask_ManageGPLinks] = tr("Manage Group Policy links");
    common_task_name[CommonTask_InetOrgPersonControl] = tr("Create, delete and manage inetOrgPerson accounts");
    common_task_name[CommonTask_ChangeInetOrgPersonPassword] = tr("Reset inetOrgPerson passwords and force password change at next logon");
    common_task_name[CommonTask_ReadAllInetOrgPersonInformation] = tr("Read all inetOrgPerson information");

    load_common_tasks_rights();
}

void ExtendedPermissionsWidget::load_common_tasks_rights() {
    // Append creation, deletion and control rights for corresponding classes
    const QHash<QString, CommonTask> creation_deletion_control_map = {
        {CLASS_USER, CommonTask_UserControl},
        {CLASS_GROUP, CommonTask_GroupControl},
        {CLASS_INET_ORG_PERSON, CommonTask_InetOrgPersonControl},
    };

    for (const QString &obj_class : creation_deletion_control_map.keys()) {
        CommonTask permission = creation_deletion_control_map[obj_class];
        common_task_rights[permission] = creation_deletion_rights_for_class(g_adconfig, obj_class) +
            control_children_class_right(g_adconfig, obj_class);
    }

    // Append "Reset password and force password change at next logon"
    // rights for user and inetOrgPerson
    const QHash<QString, CommonTask> password_change_map = {
        {CLASS_USER, CommonTask_ChangeUserPassword},
        {CLASS_INET_ORG_PERSON, CommonTask_ChangeInetOrgPersonPassword},
    };

    for (const QString &obj_class : password_change_map.keys()) {
        CommonTask permission = password_change_map[obj_class];
        common_task_rights[permission] = children_class_read_write_prop_rights(g_adconfig, obj_class, ATTRIBUTE_PWD_LAST_SET);
    }

    // Append "Read all information" rights for user and inetOrgPerson
    const QHash<QString, CommonTask> read_all_info_map = {
        {CLASS_USER, CommonTask_ReadAllUserInformation},
        {CLASS_INET_ORG_PERSON, CommonTask_ReadAllInetOrgPersonInformation},
    };

    for (const QString &obj_class : read_all_info_map.keys()) {
        CommonTask permission = read_all_info_map[obj_class];
        common_task_rights[permission] = read_all_children_class_info_rights(g_adconfig, obj_class);
    }

    // Append group membership rights.
    // NOTE: Group membership task doesnt represent "Membership" extended right.
    // These generate different ACEs: Membership task delegation doesnt include
    // memberOf attribute, that included in extended right's property set.
    // See https://learn.microsoft.com/en-us/windows/win32/adschema/r-membership.
    common_task_rights[CommonTask_GroupMembership] = children_class_read_write_prop_rights(g_adconfig, CLASS_GROUP, ATTRIBUTE_MEMBER);

    // Append group policy link manage rights
    common_task_rights[CommonTask_ManageGPLinks] =
        read_write_prop_rights(g_adconfig, ATTRIBUTE_GPOPTIONS) +
            read_write_prop_rights(g_adconfig, ATTRIBUTE_GPLINK);
}


