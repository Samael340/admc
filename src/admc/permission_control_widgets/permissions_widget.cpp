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

#include "permissions_widget.h"

#include "utils.h"
#include "settings.h"
#include "samba/ndr_security.h"
#include "ad_security.h"
#include "globals.h"

#include <QTreeView>
#include <QStandardItemModel>
#include <QLayout>
#include <QDebug>


PermissionsWidget::PermissionsWidget(QWidget *parent) :
    QWidget(parent), read_only(false) {

    rights_model = new QStandardItemModel(0, PermissionColumn_COUNT, this);
    set_horizontal_header_labels_from_map(rights_model,
        {
            {PermissionColumn_Name, tr("Name")},
            {PermissionColumn_Allowed, tr("Allowed")},
            {PermissionColumn_Denied, tr("Denied")},
        });;


    rights_view = new QTreeView(this);
    rights_view->setModel(rights_model);

    v_layout = new QVBoxLayout;
    v_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(v_layout);

    connect(
        rights_model, &QStandardItemModel::itemChanged,
        this, &PermissionsWidget::on_item_changed);

    const QLocale saved_locale = settings_get_variant(SETTING_locale).toLocale();
    language = saved_locale.language();
}

void PermissionsWidget::set_read_only() {
    read_only = true;
    make_model_rights_read_only();
}

void PermissionsWidget::set_current_trustee(const QByteArray &current_trustee) {
    trustee = current_trustee;
    update_permissions(applied_objects);
}

void PermissionsWidget::on_item_changed(QStandardItem *item) {
    if (ignore_item_changed_signal) {
        return;
    }

    const PermissionColumn column = (PermissionColumn) item->column();
    const bool incorrect_column = (column != PermissionColumn_Allowed && column != PermissionColumn_Denied);
    if (incorrect_column) {
        return;
    }

    const QModelIndex main_item_index = item->index().siblingAtColumn(0);
    QStandardItem *main_item = rights_model->itemFromIndex(main_item_index);

    const bool checked = (item->checkState() == Qt::Checked);
    const bool allow = (column == PermissionColumn_Allowed);


    const bool has_multiple_rights = !main_item->data(RightsItemRole_SecurityRightList).isNull();
    QList<SecurityRight> rights;
    if (has_multiple_rights) {
        rights = main_item->data(RightsItemRole_SecurityRightList).value<QList<SecurityRight>>();
    }
    else {
        SecurityRight right = main_item->data(RightsItemRole_SecurityRight).value<SecurityRight>();
        rights.append(right);
    }

    for (const SecurityRight &right : rights) {
        if (checked) {
            security_descriptor_add_right(sd, g_adconfig, target_class_list, trustee, right, allow);
        } else {
            security_descriptor_remove_right(sd, g_adconfig, target_class_list, trustee, right, allow);
        }
    }

    update_permissions(applied_objects);

    emit edited();
}

void PermissionsWidget::update_permissions(AppliedObjects applied_objs, const QString &appliable_child_class) {
    Q_UNUSED(applied_objs)
    Q_UNUSED(appliable_child_class)
    // TODO: Do update depending on appliable objects

    update_permissions();
}

void PermissionsWidget::update_permissions() {
    // NOTE: this flag is turned on so that
    // on_item_changed() slot doesn't react to us
    // changing state of items
    ignore_item_changed_signal = true;

    for (int row = 0; row < rights_model->rowCount(); row++) {
        const QModelIndex index = rights_model->index(row, 0);
        if (!index.isValid()) {
            continue;
        }

        QStandardItem *main_item = rights_model->itemFromIndex(index);

        QList<SecurityRight> rights_list;
        // Common tasks are stored as right lists
        const bool is_common_task = !main_item->data(RightsItemRole_SecurityRightList).isNull();
        if (is_common_task) {
            rights_list = main_item->data(RightsItemRole_SecurityRightList).value<QList<SecurityRight>>();
        }
        else {
            SecurityRight right = main_item->data(RightsItemRole_SecurityRight).value<SecurityRight>();
            rights_list.append(right);
        }

        // Do not disable checkboxes for common tasks, because the task delegated in the parent
        // container can be also delegated in the child container. New ACEs are put on top of
        // the inherited from parent ones.
        update_row_check_state(row, rights_list, is_common_task);
    }

    // NOTE: need to make read only again because
    // during load, items are enabled/disabled based on
    // their inheritance state
    if (read_only) {
        make_model_rights_read_only();
    }

    ignore_item_changed_signal = false;
}

void PermissionsWidget::make_model_rights_read_only() {
    // NOTE: important to ignore this signal because
    // it's slot reloads the rights model
    ignore_item_changed_signal = true;

    for (int row = 0; row < rights_model->rowCount(); row++) {
        const QList<int> col_list = {
            PermissionColumn_Allowed,
            PermissionColumn_Denied,
        };

        for (const int col : col_list) {
            QStandardItem *item = rights_model->item(row, col);
            item->setEnabled(false);
        }
    }

    ignore_item_changed_signal = false;
}

void PermissionsWidget::update_row_check_state(int row, QList<SecurityRight> &rights, bool ignore_inheritance) {
    const QHash<SecurityRightStateType, QModelIndex> checkable_index_map = {
        {SecurityRightStateType_Allow, rights_model->index(row, PermissionColumn_Allowed)},
        {SecurityRightStateType_Deny, rights_model->index(row, PermissionColumn_Denied)},
    };

    // For items with multiple rights (some common tasks)
    // states have to match
    QHash<SecurityRightStateType, Qt::CheckState> right_state_checked_map;

    for (const SecurityRight &right : rights) {
        const SecurityRightState state = security_descriptor_get_right_state(sd, trustee, right);
        for (int type_i = 0; type_i < SecurityRightStateType_COUNT; type_i++) {
            const SecurityRightStateType type = (SecurityRightStateType) type_i;

            if (right_state_checked_map.contains(type) &&
                    right_state_checked_map[type] == Qt::Unchecked) {
                continue;
            }

            QStandardItem *item = rights_model->itemFromIndex(checkable_index_map[type]);

            const bool object_ace_state = state.get(SecurityRightStateInherited_No, type);
            const bool inherited_ace_state = state.get(SecurityRightStateInherited_Yes, type) && !ignore_inheritance;

            // Checkboxes become disabled if they
            // contain only inherited state. Note that
            // if there's both inherited and object
            // state for same right, checkbox is
            // enabled so that user can remove object
            // state.
            const bool disabled = (inherited_ace_state && !object_ace_state);
            item->setEnabled(!disabled);

            Qt::CheckState check_state = object_ace_state || inherited_ace_state ? Qt::Checked : Qt::Unchecked;
            item->setCheckState(check_state);
            right_state_checked_map[type] = check_state;
        }
    }
}
