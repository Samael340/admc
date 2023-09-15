/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2023 BaseALT Ltd.
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

#include "domain_info_results_widget.h"
#include "ui_domain_info_results_widget.h"
#include "console_widget/console_widget.h"
#include "ad_interface.h"
#include "settings.h"
#include "ad_defines.h"
#include "console_impls/object_impl.h"
#include "console_impls/item_type.h"
#include "globals.h"
#include "ad_config.h"
#include "status.h"
#include "fsmo/fsmo_utils.h"
#include "icon_manager/icon_manager.h"
#include "utils.h"

#include <QStandardItemModel>
#include <QPushButton>
#include <QtConcurrent/QtConcurrent>
#include <QDebug>

DomainInfoResultsWidget::DomainInfoResultsWidget(ConsoleWidget *console_arg) :
    QWidget(console_arg), ui(new Ui::DomainInfoResultsWidget), console(console_arg) {
    ui->setupUi(this);

    model = new QStandardItemModel(this);
    ui->tree->setModel(model);
    ui->tree->setHeaderHidden(true);

    ui->updating_label->setVisible(false);

    connect(console, &ConsoleWidget::fsmo_master_changed, this, &DomainInfoResultsWidget::update_fsmo_roles);
    connect(ui->update_button, &QPushButton::clicked, this, &DomainInfoResultsWidget::update_button_clicked);
}

DomainInfoResultsWidget::~DomainInfoResultsWidget() {
    delete ui;
}

void DomainInfoResultsWidget::update(AdInterface &ad) {
    update_defaults();

    obj_search_watcher.setFuture(QtConcurrent::run(this, &DomainInfoResultsWidget::search_results));

    connect(&obj_search_watcher, &QFutureWatcher<DomainInfo_SearchResults>::started, [this](){
        ui->update_button->setDisabled(true);
        ui->updating_label->setVisible(true);
        show_busy_indicator();
    });

    connect(&obj_search_watcher, &QFutureWatcher<DomainInfo_SearchResults>::finished, [this](){
        populate_widgets(obj_search_watcher.result());
        ui->update_button->setDisabled(false);
        ui->updating_label->setVisible(false);
        hide_busy_indicator();
    });
}

void DomainInfoResultsWidget::update_fsmo_roles(const QString &new_master_dn, const QString &fsmo_role_string) {
    QList<QStandardItem*> fsmo_role_item_list = model->findItems(fsmo_role_string, Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive));
    if (fsmo_role_item_list.isEmpty()) {
        return;
    }

    const QModelIndex start_index = model->index(0, 0, QModelIndex());
    QModelIndexList new_master_index_list = model->match(start_index, DomainInfoTreeItemRole_DN,
                                                         new_master_dn, 1, Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive));
    if (new_master_index_list.isEmpty()) {
        return;
    }

    QStandardItem *fsmo_role_item = fsmo_role_item_list[0];
    QModelIndex new_master_index = new_master_index_list[0];
    QModelIndex roles_container_index = model->match(new_master_index, DomainInfoTreeItemRole_ItemType, DomainInfoTreeItemType_FSMO_Container,
                                                     1, Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive))[0];
    model->itemFromIndex(roles_container_index)->appendRow(
                fsmo_role_item->parent()->takeRow(fsmo_role_item->row()));
}

void DomainInfoResultsWidget::update_defaults() {
    model->clear();
    ui->sites_count_sbox->setValue(0);
    ui->dc_count_sbox->setValue(0);
    ui->ou_count_sbox->setValue(0);
    ui->gpo_count_sbox->setValue(0);
    ui->users_count_sbox->setValue(0);
    ui->computers_count_sbox->setValue(0);
    ui->groups_count_sbox->setValue(0);
}

QList<QStandardItem *> DomainInfoResultsWidget::get_tree_items(AdInterface &ad) {
    const QString sites_container_dn = "CN=Sites,CN=Configuration," + g_adconfig->domain_dn();
    const QString filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, CLASS_SITE);
    const QHash<QString, AdObject> site_objects = ad.search(sites_container_dn, SearchScope_Children,
                                                            filter, {ATTRIBUTE_DN, ATTRIBUTE_NAME/*, ATTRIBUTE_GPLINK*/});

    QList<QStandardItem *> site_items;
    if (site_objects.isEmpty()) {
        g_status->add_message("Failed to find domain sites.", StatusType_Error);
        return site_items;
    }

    for (const AdObject &site_object : site_objects.values()) {
        QStandardItem *site_item = add_tree_item(site_object.get_string(ATTRIBUTE_NAME), g_icon_manager->get_icon_for_type(ItemIconType_Site_Clean),
                                                 DomainInfoTreeItemType_Site, nullptr);
        site_item->setData(site_object.get_dn(), DomainInfoTreeItemRole_DN);

        add_host_items(site_item, site_object, ad);

        site_items.append(site_item);
    }

    return site_items;
}

QHash<QString, int> DomainInfoResultsWidget::get_objects_count(AdInterface &ad) {
    QHash<QString, int> obj_count_hash = {
        {CLASS_OU, 0},
        {CLASS_GP_CONTAINER, 0},
        {CLASS_USER, 0},
        {CLASS_COMPUTER, 0},
        {CLASS_GROUP, 0}
    };

    for (const QString &obj_class : obj_count_hash.keys()) {
        int count = 0;
        if (ad.search_objects_count(obj_class, &count)) {
            obj_count_hash[obj_class] = count;
        }
        else {
            obj_count_hash.remove(obj_class);
        }
    }

    return obj_count_hash;
}

DomainInfo_SearchResults DomainInfoResultsWidget::search_results() {
    AdInterface ad;
    DomainInfo_SearchResults res {
      get_objects_count(ad),
      get_tree_items(ad)
    };
    return res;
}

void DomainInfoResultsWidget::populate_widgets(DomainInfo_SearchResults results) {
    for (auto item : results.tree_items) {
        model->appendRow(item);
    }
    set_spinbox_failed(ui->sites_count_sbox, false);
    set_spinbox_failed(ui->dc_count_sbox, false);

    // Set dc and sites count to spinbox values
    // Search is performed in the tree to avoid excessive ldap queries
    const QHash<int, QSpinBox*> type_spinbox_hash {
        {DomainInfoTreeItemType_Site, ui->sites_count_sbox},
        {DomainInfoTreeItemType_Host, ui->dc_count_sbox}
    };

    for (int type : type_spinbox_hash.keys()) {
        const QModelIndex start_index = model->index(0, 0, QModelIndex());
        int count = model->match(start_index, DomainInfoTreeItemRole_ItemType, type,
                             -1, Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive)).count();
        if (!count) {
            set_spinbox_failed(type_spinbox_hash[type], true);
        }
        type_spinbox_hash[type]->setValue(count);
    }

    // Set remaining objects count to corresponding spinboxes
    const QHash<QString, QSpinBox*> class_spinbox_hash {
      {CLASS_OU, ui->ou_count_sbox},
      {CLASS_GP_CONTAINER, ui->gpo_count_sbox},
      {CLASS_USER, ui->users_count_sbox},
      {CLASS_COMPUTER, ui->computers_count_sbox},
      {CLASS_GROUP, ui->groups_count_sbox}
    };

    for (const QString &obj_class : class_spinbox_hash.keys()) {
        const QHash<QString, int> count_results = results.objects_count_hash;
        if (count_results.contains(obj_class)) {
            class_spinbox_hash[obj_class]->setValue(count_results[obj_class]);
            set_spinbox_failed(class_spinbox_hash[obj_class], false);
        }
        else {
            set_spinbox_failed(class_spinbox_hash[obj_class], true);
        }
    }
}

void DomainInfoResultsWidget::add_host_items(QStandardItem *site_item, const AdObject &site_object, AdInterface &ad) {
    const QString servers_container_dn = "CN=Servers," + site_object.get_dn();
    const QString filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, CLASS_SERVER);
    const QHash<QString, AdObject> host_objects = ad.search(servers_container_dn, SearchScope_Children,
                                                            filter, {ATTRIBUTE_DN, ATTRIBUTE_NAME, ATTRIBUTE_DNS_HOST_NAME});
    QStandardItem *servers_item = add_tree_item(tr("Servers"), g_icon_manager->get_object_icon("Container"),
                                                DomainInfoTreeItemType_ServersContainer, site_item);

    QHash<QString, QStringList> host_fsmo_map;
    for (int role = 0; role < FSMORole_COUNT; ++role) {
        const QString host = current_master_for_role(ad, FSMORole(role));
        host_fsmo_map[host].append(string_fsmo_role(FSMORole(role)));
    }
    QString host;
    for (const AdObject &host_object : host_objects.values()) {
        QStandardItem *host_item = add_tree_item(host_object.get_string(ATTRIBUTE_DNS_HOST_NAME), g_icon_manager->get_icon_for_type(ItemIconType_Domain_Clean),
                                                 DomainInfoTreeItemType_Host, servers_item);
        host_item->setData(host_object.get_dn(), DomainInfoTreeItemRole_DN);
        host = host_object.get_string(ATTRIBUTE_DNS_HOST_NAME);

        QStandardItem *fsmo_roles_container_item = add_tree_item(tr("FSMO roles"), QIcon::fromTheme("applications-system"),
                                                                 DomainInfoTreeItemType_FSMO_Container, host_item);

        if (host_fsmo_map.keys().contains(host_item->text())) {
            for (const QString &fsmo_role : host_fsmo_map[host_item->text()]) {
                add_tree_item(fsmo_role, g_icon_manager->get_object_icon("Configuration"),
                              DomainInfoTreeItemType_FSMO_Role,fsmo_roles_container_item);
            }
        }
    }
}

void DomainInfoResultsWidget::update_button_clicked() {
    AdInterface ad;
    if (!ad.is_connected()) {
        return;
    }
    update(ad);
}

void DomainInfoResultsWidget::set_spinbox_failed(QSpinBox *spinbox, bool failed) {
    if (failed) {
        spinbox->setStyleSheet("color: red");
        spinbox->setToolTip(tr("Failed to get objects count"));
    }
    else {
        spinbox->setStyleSheet("");
        spinbox->setToolTip(tr(""));
    }
}

QStandardItem* DomainInfoResultsWidget::add_tree_item(const QString &text, const QIcon &icon, DomainInfoTreeItemType type, QStandardItem *parent_item) {
    QStandardItem *item = new QStandardItem(icon, text);
    item->setEditable(false);
    item->setData(type, DomainInfoTreeItemRole_ItemType);
    if (parent_item) {
        parent_item->appendRow(item);
    }
    return item;
}
