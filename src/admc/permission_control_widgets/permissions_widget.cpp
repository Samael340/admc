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

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(rights_view);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    connect(
        rights_model, &QStandardItemModel::itemChanged,
        this, &PermissionsWidget::on_item_changed);

    const QLocale saved_locale = settings_get_variant(SETTING_locale).toLocale();
    language = saved_locale.language();
}

void PermissionsWidget::set_read_only() {
    read_only = true;
    make_model_rights_read_only(QModelIndex());
}

void PermissionsWidget::set_current_trustee(const QByteArray &current_trustee) {
    trustee = current_trustee;
    update_model_right_items(QModelIndex());
}

void PermissionsWidget::on_item_changed(QStandardItem *item) {
    if (ignore_item_changed_signal) {
        return;
    }

    qDebug() << "Item index: " << item->index().row() << " : " << item->column();
    const PermissionColumn column = (PermissionColumn) item->column();
    const bool incorrect_column = (column != PermissionColumn_Allowed && column != PermissionColumn_Denied);
    if (incorrect_column) {
        return;
    }

    const QModelIndex main_item_index = item->index().siblingAtColumn(PermissionColumn_Name);
    QStandardItem *main_item = rights_model->itemFromIndex(main_item_index);

    const bool checked = (item->checkState() == Qt::Checked);
    const bool allow = (column == PermissionColumn_Allowed);
    QList<SecurityRight> rights = main_item->data(RightsItemRole_SecurityRights).
            value<QList<SecurityRight>>();

    for (const SecurityRight &right : rights) {
        if (checked) {
            security_descriptor_add_right(sd, g_adconfig, target_class_list, trustee, right, allow);
        } else {
            security_descriptor_remove_right(sd, g_adconfig, target_class_list, trustee, right, allow);
        }
    }

    const QModelIndex parent_index = item->parent() == nullptr ? QModelIndex() : item->parent()->index();
    update_model_right_items(parent_index);

    emit edited();
}

void PermissionsWidget::update_model_right_items(const QModelIndex &parent) {
    // NOTE: this flag is turned on so that
    // on_item_changed() slot doesn't react to us
    // changing state of items
    ignore_item_changed_signal = true;

    for (int row = 0; row < rights_model->rowCount(parent); row++) {
        const QModelIndex index = rights_model->index(row, 0, parent);
        if (!index.isValid()) {
            continue;
        }

        const QHash<SecurityRightStateType, QModelIndex> checkable_index_map = {
            {SecurityRightStateType_Allow, rights_model->index(row, PermissionColumn_Allowed, parent)},
            {SecurityRightStateType_Deny, rights_model->index(row, PermissionColumn_Denied, parent)},
        };

        QStandardItem *main_item = rights_model->itemFromIndex(index);
        QList<SecurityRight> rights = main_item->data(RightsItemRole_SecurityRights).
                value<QList<SecurityRight>>();
        for (const SecurityRight right : rights) {
            const SecurityRightState state = security_descriptor_get_right_state(sd, trustee, right);
            for (int type_i = 0; type_i < SecurityRightStateType_COUNT; type_i++) {
                const SecurityRightStateType type = (SecurityRightStateType) type_i;

                QStandardItem *item = rights_model->itemFromIndex(checkable_index_map[type]);

                const bool object_ace_state = state.get(SecurityRightStateInherited_No, type);
                const bool inherited_ace_state = state.get(SecurityRightStateInherited_Yes, type);

                // Checkboxes become disabled if they
                // contain only inherited state. Note that
                // if there's both inherited and object
                // state for same right, checkbox is
                // enabled so that user can remove object
                // state.
                const bool disabled = (inherited_ace_state && !object_ace_state);
                item->setEnabled(!disabled);

                const Qt::CheckState check_state = [&]() {
                    if (object_ace_state || inherited_ace_state) {
                        return Qt::Checked;
                    } else {
                        return Qt::Unchecked;
                    }
                }();
                item->setCheckState(check_state);
            }
        }
    }

    // NOTE: need to make read only again because
    // during load, items are enabled/disabled based on
    // their inheritance state
    if (read_only) {
        make_model_rights_read_only(QModelIndex());
    }

    ignore_item_changed_signal = false;
}

void PermissionsWidget::make_model_rights_read_only(const QModelIndex &parent) {
    // NOTE: important to ignore this signal because
    // it's slot reloads the rights model
    ignore_item_changed_signal = true;

    for (int row = 0; row < rights_model->rowCount(parent); row++) {
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
