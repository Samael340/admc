#include "common_permissions_widget.h"

#include "adldap.h"
#include "globals.h"
#include "settings.h"
#include "utils.h"

#include "samba/ndr_security.h"

#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QTreeView>
#include <QDebug>

class CommonRightsSortModel final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        const uint32_t access_mask_left = source_left.data(RightsItemRole_AccessMask).toUInt();
        const uint32_t access_mask_right = source_right.data(RightsItemRole_AccessMask).toUInt();

        // Generic among generic are in pre-defined order
        const int common_index_left = common_rights_list.indexOf(access_mask_left);
        const int common_index_right = common_rights_list.indexOf(access_mask_right);

        return (common_index_left < common_index_right);
    }
};

CommonPermissionsWidget::CommonPermissionsWidget(QWidget *parent) : PermissionsWidget(parent) {
    rights_sort_model = new CommonRightsSortModel(this);
    rights_sort_model->setSourceModel(rights_model);

    rights_view->setModel(rights_sort_model);
    rights_view->setColumnWidth(AceColumn_Name, 400);

    settings_restore_header_state(SETTING_common_permissions_header_state, rights_view->header());
}

CommonPermissionsWidget::~CommonPermissionsWidget() {
    settings_save_header_state(SETTING_common_permissions_header_state, rights_view->header());
}

void CommonPermissionsWidget::init(const QStringList &target_classes, security_descriptor *sd_arg) {
    sd = sd_arg;
    target_class_list = target_classes;
    rights_model->removeRows(0, rights_model->rowCount());

    // Create items in rights model. These will not
    // change until target object changes. Only the
    // state of items changes during editing.
    const QList<SecurityRight> right_list = ad_security_get_common_rights();
    for (const SecurityRight &right : right_list) {
        const QList<QStandardItem *> row = make_item_row(AceColumn_COUNT);

        // TODO: for russian, probably do "read/write
        // property - [property name]" to avoid having
        // to do suffixes properties
        const QString right_name = ad_security_get_right_name(g_adconfig, right, language);

        row[AceColumn_Name]->setText(right_name);
        row[AceColumn_Allowed]->setCheckable(true);
        row[AceColumn_Denied]->setCheckable(true);

        row[0]->setData(right.access_mask, RightsItemRole_AccessMask);
        row[0]->setData(right.object_type, RightsItemRole_ObjectType);

        rights_model->appendRow(row);
    }

    // NOTE: because rights model is dynamically filled
    // when trustee switches, we have to make rights
    // model read only again after it's reloaded
    if (read_only) {
        make_model_rights_read_only(QModelIndex());
    }

    rights_sort_model->sort(0);
}
