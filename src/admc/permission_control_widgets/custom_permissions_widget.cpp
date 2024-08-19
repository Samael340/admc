#include "custom_permissions_widget.h"

#include "adldap.h"
#include "globals.h"
#include "settings.h"
#include "utils.h"

#include "samba/ndr_security.h"

#include <QStandardItemModel>
#include <QStandardItem>
#include <QTreeView>


CustomPermissionsWidget::CustomPermissionsWidget(QWidget *parent) {

}

CustomPermissionsWidget::~CustomPermissionsWidget() {

}

void CustomPermissionsWidget::init(const QStringList &target_classes, security_descriptor *sd_arg) {

}

void CustomPermissionsWidget::on_item_changed(QStandardItem *item) {

}

void CustomPermissionsWidget::update_model_rights(const QModelIndex &index) {

}
