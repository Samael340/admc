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

    rights_model = new QStandardItemModel(0, AceColumn_COUNT, this);
    set_horizontal_header_labels_from_map(rights_model,
        {
            {AceColumn_Name, tr("Name")},
            {AceColumn_Allowed, tr("Allowed")},
            {AceColumn_Denied, tr("Denied")},
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
    update_model_rights(QModelIndex());
}

void PermissionsWidget::on_item_changed(QStandardItem *item) {
    if (ignore_item_changed_signal) {
        return;
    }

    qDebug() << "Item index: " << item->index().row() << " : " << item->column();
    const AceColumn column = (AceColumn) item->column();
    const bool incorrect_column = (column != AceColumn_Allowed && column != AceColumn_Denied);
    if (incorrect_column) {
        return;
    }

    const QModelIndex main_item_index = item->index().siblingAtColumn(AceColumn_Name);
    QStandardItem *main_item = rights_model->itemFromIndex(main_item_index);

    uint8_t flags = 0;
    const QVariant flags_data = main_item->data(RightsItemRole_ACE_Flags);
    if (flags_data.isValid()) {
        flags = (uint8_t)flags_data.toInt();
    }

    const bool checked = (item->checkState() == Qt::Checked);

    const uint32_t access_mask = main_item->data(RightsItemRole_AccessMask).toUInt();
    const QByteArray object_type = main_item->data(RightsItemRole_ObjectType).toByteArray();
    const bool allow = (column == AceColumn_Allowed);

    SecurityRight right{access_mask, object_type, QByteArray(), 0};

    if (checked) {
        security_descriptor_add_right(sd, g_adconfig, target_class_list, trustee, right, allow);
    } else {
        security_descriptor_remove_right(sd, g_adconfig, target_class_list, trustee, right, allow);
    }

    const QModelIndex parent_index = item->parent() == nullptr ? QModelIndex() : item->parent()->index();
    update_model_rights(parent_index);

    emit edited();
}

void PermissionsWidget::update_model_rights(const QModelIndex &parent) {
    // NOTE: this flag is turned on so that
    // on_item_changed() slot doesn't react to us
    // changing state of items
    ignore_item_changed_signal = true;

    for (int row = 0; row < rights_model->rowCount(parent); row++) {
        const QModelIndex index = rights_model->index(row, 0, parent);
        if (!index.isValid()) {
            continue;
        }
        QStandardItem *main_item = rights_model->itemFromIndex(index);
        const uint32_t access_mask = main_item->data(RightsItemRole_AccessMask).toUInt();
        const QByteArray object_type = main_item->data(RightsItemRole_ObjectType).toByteArray();
        SecurityRight right{access_mask, object_type, QByteArray(), 0};
        const SecurityRightState state = security_descriptor_get_right(sd, trustee, right);

        const QHash<SecurityRightStateType, QModelIndex> checkable_index_map = {
            {SecurityRightStateType_Allow, rights_model->index(row, AceColumn_Allowed, parent)},
            {SecurityRightStateType_Deny, rights_model->index(row, AceColumn_Denied, parent)},
        };

        for (int type_i = 0; type_i < SecurityRightStateType_COUNT; type_i++) {
            const SecurityRightStateType type = (SecurityRightStateType) type_i;

            QStandardItem *item = rights_model->itemFromIndex(checkable_index_map[type]);

            const bool object_state = state.get(SecurityRightStateInherited_No, type);
            const bool inherited_state = state.get(SecurityRightStateInherited_Yes, type);

            // Checkboxes become disabled if they
            // contain only inherited state. Note that
            // if there's both inherited and object
            // state for same right, checkbox is
            // enabled so that user can remove object
            // state.
            const bool disabled = (inherited_state && !object_state);
            item->setEnabled(!disabled);

            const Qt::CheckState check_state = [&]() {
                if (object_state || inherited_state) {
                    return Qt::Checked;
                } else {
                    return Qt::Unchecked;
                }
            }();
            item->setCheckState(check_state);
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
            AceColumn_Allowed,
            AceColumn_Denied,
        };

        for (const int col : col_list) {
            QStandardItem *item = rights_model->item(row, col);
            item->setEnabled(false);
        }
    }

    ignore_item_changed_signal = false;
}
