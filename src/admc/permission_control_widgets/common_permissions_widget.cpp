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

#include "common_permissions_widget.h"

#include "adldap.h"
#include "globals.h"
#include "settings.h"
#include "utils.h"

#include "samba/ndr_security.h"

#include <QStandardItemModel>
#include <QStandardItem>
#include <QTreeView>
#include <QVBoxLayout>
#include <QDebug>

class CommonRightsSortModel final : public RightsSortModel {
public:
    using RightsSortModel::RightsSortModel;

    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        SecurityRight left = source_left.data(RightsItemRole_SecurityRight).
                value<SecurityRight>();
        SecurityRight right = source_right.data(RightsItemRole_SecurityRight).
                value<SecurityRight>();
        const uint32_t access_mask_left = left.access_mask;
        const uint32_t access_mask_right = right.access_mask;

        // Generic among generic are in pre-defined order
        const int common_index_left = common_rights_list.indexOf(access_mask_left);
        const int common_index_right = common_rights_list.indexOf(access_mask_right);

        return (common_index_left < common_index_right);
    }
};

CommonPermissionsWidget::CommonPermissionsWidget(QWidget *parent) : PermissionsWidget(parent) {
    v_layout->addWidget(rights_view);

    rights_sort_model = new CommonRightsSortModel(this);
    rights_sort_model->setSourceModel(rights_model);

    rights_view->setModel(rights_sort_model);
    rights_view->setColumnWidth(PermissionColumn_Name, 400);

    settings_restore_header_state(SETTING_common_permissions_header_state, rights_view->header());
}

CommonPermissionsWidget::~CommonPermissionsWidget() {
    settings_save_header_state(SETTING_common_permissions_header_state, rights_view->header());
}

void CommonPermissionsWidget::init(const QStringList &target_classes, security_descriptor *sd_arg) {
    PermissionsWidget::init(target_classes, sd_arg);

    // Create items in rights model. These will not
    // change until target object changes. Only the
    // state of items changes during editing.
    const QList<SecurityRight> right_list = ad_security_get_common_rights();
    for (const SecurityRight &right : right_list) {
        const QList<QStandardItem *> row = make_item_row(PermissionColumn_COUNT);

        // TODO: for russian, probably do "read/write
        // property - [property name]" to avoid having
        // to do suffixes properties
        const QString right_name = ad_security_get_right_name(g_adconfig, right, language);

        row[PermissionColumn_Name]->setText(right_name);
        row[PermissionColumn_Name]->setEditable(false);
        row[PermissionColumn_Allowed]->setCheckable(true);
        row[PermissionColumn_Allowed]->setEditable(false);
        row[PermissionColumn_Denied]->setCheckable(true);
        row[PermissionColumn_Denied]->setEditable(false);

        QVariant right_data;
        right_data.setValue(right);
        row[0]->setData(right_data, RightsItemRole_SecurityRight);

        rights_model->appendRow(row);
    }

    // NOTE: because rights model is dynamically filled
    // when trustee switches, we have to make rights
    // model read only again after it's reloaded
    if (read_only) {
        make_model_rights_read_only();
    }

    rights_sort_model->sort(0);
}

void CommonPermissionsWidget::update_permissions(AppliedObjects applied_objs, const QString &appliable_child_class) {
    applied_objects = applied_objs;
    ignore_item_changed_signal = true;

    for (int row = 0; row < rights_model->rowCount(); row++) {
        const QModelIndex index = rights_model->index(row, 0);

        if (item_is_message(index)) {
            continue;
        }

        SecurityRight right = rights_model->data(index, RightsItemRole_SecurityRight).value<SecurityRight>();

        switch (applied_objs) {
        case AppliedObjects_ThisObject:
            right.flags = 0;
            right.inherited_object_type = QByteArray();
            break;

        case AppliedObjects_ThisAndChildObjects:
            right.flags = SEC_ACE_FLAG_CONTAINER_INHERIT;
            right.inherited_object_type = QByteArray();
            break;

        case AppliedObjects_AllChildObjects:
            right.flags = SEC_ACE_FLAG_CONTAINER_INHERIT | SEC_ACE_FLAG_INHERIT_ONLY;
            right.inherited_object_type = QByteArray();
            break;

        case AppliedObjects_ChildObjectClass:
        {
            const QList<uint32_t> parent_related_rights = {
                SEC_ADS_DELETE_TREE,
                SEC_ADS_CREATE_CHILD,
                SEC_ADS_DELETE_CHILD
            };
            // Ignore parent specific rights for classes with no possible inferiors
            if (g_adconfig->get_possible_inferiors(appliable_child_class).isEmpty() &&
                    parent_related_rights.contains(right.access_mask)) {
                rights_model->setData(index, true, RightsItemRole_HiddenItem);
                continue;
            }

            rights_model->setData(index, false, RightsItemRole_HiddenItem);
            right.flags = SEC_ACE_FLAG_CONTAINER_INHERIT | SEC_ACE_FLAG_INHERIT_ONLY;
            right.inherited_object_type = g_adconfig->guid_from_class(appliable_child_class);
            break;
        }

        default:
            ignore_item_changed_signal = false;
            return;
        }

        QVariant right_data;
        right_data.setValue(right);
        rights_model->setData(index, right_data, RightsItemRole_SecurityRight);
    }

    rights_sort_model->hide_ignored_items();
    ignore_item_changed_signal = false;

    PermissionsWidget::update_permissions();
}

