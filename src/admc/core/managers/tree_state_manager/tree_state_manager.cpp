/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2026 BaseALT Ltd.
 * Copyright (C) 2026 Semyon Knyazev
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

#include "tree_state_manager.h"
#include "ui/widget/console/console_widget.h"
#include <QSettings>
#include "ad_config.h"
#include "core/globals.h"
#include "ui/console/object/console_object_operations.h"
#include "ui/console/policy_root_impl.h"
#include "ui/console/policy_impl.h"
#include "ui/console/policy_ou_impl.h"
#include "ui/console/all_policies_folder_impl.h"
#include "ui/console/query_folder_impl.h"
#include <QTreeView>
#include "core/utils.h"
#include "ad_utils.h"
#include "core/settings.h"
#include <QAbstractProxyModel>
#include <QAbstractItemModel>
#include <QDebug>


TreeStateManager::TreeStateManager(ConsoleWidget *console, QTreeView *view, const QString &domain, const QString &user) :
    console_(console), view_(view), domain_(domain), user_(user),
tree_state_prefix("tree_state/" + user + "/" + domain + "/") {
    proxy_model = qobject_cast<QAbstractProxyModel*>(view_->model());
    source_model = proxy_model ? proxy_model->sourceModel() : nullptr;

    qInfo() << "[TreeStateManager] ctor: domain=" << domain_ << "user=" << user_
            << "proxy_model=" << (proxy_model != nullptr) << "source_model=" << (source_model != nullptr)
            << "prefix=" << tree_state_prefix;
}

void TreeStateManager::set_context(const QString &domain, const QString &user) {
    qInfo() << "[TreeStateManager] set_context: old domain=" << domain_ << "old user=" << user_
            << "-> new domain=" << domain << "new user=" << user;
    domain_ = domain;
    user_ = user;
    tree_state_prefix = "tree_state/" + user + "/" + domain + "/";
    qInfo() << "[TreeStateManager] set_context: new prefix=" << tree_state_prefix;
}

void TreeStateManager::save() {
    qInfo() << "[TreeStateManager] save() called: domain=" << domain_ << "user=" << user_;
    if (domain_.isEmpty() || user_.isEmpty() || !console_ || !view_ || !proxy_model || !source_model) {
        qInfo() << "[TreeStateManager] save() aborted: domain_empty=" << domain_.isEmpty()
        << "user_empty=" << user_.isEmpty() << "console=" << (console_ != nullptr)
        << "view=" << (view_ != nullptr) << "proxy_model=" << (proxy_model != nullptr)
        << "source_model=" << (source_model != nullptr);
        return;
    }

    qInfo() << "[TreeStateManager] save(): saving object subtree";
    save_object_subtree(ConsoleObjectTreeOperations::get_domain_object_tree_root(console_),
        object_tree_key);
    qInfo() << "[TreeStateManager] save(): saving policy subtree";
    save_policy_subtree();
    save_pso_subtree();
    qInfo() << "[TreeStateManager] save(): saving sites subtree";
    save_object_subtree(ConsoleObjectTreeOperations::get_sites_container_tree_root(console_),
        sites_tree_key);
    qInfo() << "[TreeStateManager] save(): saving query subtree";
    save_query_subtree();
    qInfo() << "[TreeStateManager] save(): done (NOTE: save_pso_subtree() and "
               "save_current_selected_item() are NOT called here)";
}

void TreeStateManager::restore() {
    const bool context_match = g_adconfig->domain() == domain_ &&
                               g_adconfig->user() == user_;
    qInfo() << "[TreeStateManager] restore() called: domain_=" << domain_ << "user_=" << user_
            << "g_adconfig->domain()=" << g_adconfig->domain() << "g_adconfig->user()=" << g_adconfig->user()
            << "context_match=" << context_match;
    if (!proxy_model || !source_model || !console_ || !view_ || !context_match) {
        qInfo() << "[TreeStateManager] restore() aborted: proxy_model=" << (proxy_model != nullptr)
        << "source_model=" << (source_model != nullptr) << "console=" << (console_ != nullptr)
        << "view=" << (view_ != nullptr) << "context_match=" << context_match;
        return;
    }

    qInfo() << "[TreeStateManager] restore(): restoring object subtree";
    restore_object_subtree();
    qInfo() << "[TreeStateManager] restore(): restoring policy subtree";
    restore_policy_subtree();
    qInfo() << "[TreeStateManager] restore(): restoring pso subtree";
    restore_pso_subtree();
    qInfo() << "[TreeStateManager] restore(): restoring sites subtree";
    restore_sites_subtree();
    qInfo() << "[TreeStateManager] restore(): restoring queries subtree";
    restore_queries_subtree();
    qInfo() << "[TreeStateManager] restore(): restoring current selected item";
    restore_current_selected_item();
    qInfo() << "[TreeStateManager] restore(): done";
}

void TreeStateManager::save_object_subtree(const QModelIndex &parent, const QString &key) {
    qInfo() << "[TreeStateManager] save_object_subtree: key=" << key << "parent_valid=" << parent.isValid();
    if (!parent.isValid()) {
        qInfo() << "[TreeStateManager] save_object_subtree: aborted, invalid parent for key=" << key;
        return;
    }

    auto expanded_list = expanded_index_list(parent);
    QStringList expanded_dn_list = index_list_to_dn_list(expanded_list, ObjectRole_DN);
    qInfo() << "[TreeStateManager] save_object_subtree: key=" << key
            << "expanded_count=" << expanded_list.size() << "dn_count=" << expanded_dn_list.size()
            << "dns=" << expanded_dn_list;

    QSettings settings;
    settings.setValue(tree_state_prefix + key, expanded_dn_list);
    qInfo() << "[TreeStateManager] save_object_subtree: written to key="
            << (tree_state_prefix + key);
}

void TreeStateManager::save_policy_subtree() {
    const QModelIndex policy_root = ConsoleObjectTreeOperations::get_domain_object_tree_root(console_);
    qInfo() << "[TreeStateManager] save_policy_subtree: policy_root_valid=" << policy_root.isValid();
    if (!policy_root.isValid()) {
        qInfo() << "[TreeStateManager] save_policy_subtree: aborted, invalid policy_root";
        return;
    }

    QSettings settings;

    auto expanded_list = expanded_index_list(policy_root);
    qInfo() << "[TreeStateManager] save_policy_subtree: raw expanded_count=" << expanded_list.size();

    const auto all_policies_folder_idx = get_all_policies_folder_index(console_);
    if (all_policies_folder_idx.isValid() && expanded_list.contains(all_policies_folder_idx)) {
        qInfo() << "[TreeStateManager] save_policy_subtree: all_policies_folder is expanded, key="
                << (tree_state_prefix + policy_tree_key + all_policies_folder_key);
        settings.setValue(tree_state_prefix + policy_tree_key + all_policies_folder_key, true);
        expanded_list.removeAll(all_policies_folder_idx);
    }
    else {
        qInfo() << "[TreeStateManager] save_policy_subtree: all_policies_folder is NOT expanded, key="
                << (tree_state_prefix + policy_tree_key + all_policies_folder_key);
        settings.setValue(tree_state_prefix + policy_tree_key + all_policies_folder_key, false);
    }

    expanded_list.removeAll(policy_root);

    QStringList expanded_dn_list = index_list_to_dn_list(expanded_list, PolicyOURole_DN);
    qInfo() << "[TreeStateManager] save_policy_subtree: final dn_count=" << expanded_dn_list.size()
            << "dns=" << expanded_dn_list << "key=" << (tree_state_prefix + policy_tree_key);
    settings.setValue(tree_state_prefix + policy_tree_key, expanded_dn_list);
}

void TreeStateManager::save_pso_subtree() {
    const QModelIndex pso_root_proxy_idx = proxy_model->mapFromSource(
        ConsoleObjectTreeOperations::get_pso_container_tree_root(console_));
    qInfo() << "[TreeStateManager] save_pso_subtree: pso_root_proxy_valid=" << pso_root_proxy_idx.isValid();
    if (!pso_root_proxy_idx.isValid()) {
        qInfo() << "[TreeStateManager] save_pso_subtree: aborted, invalid pso root";
        return;
    }

    const bool expanded = view_->isExpanded(pso_root_proxy_idx);
    qInfo() << "[TreeStateManager] save_pso_subtree: expanded=" << expanded
            << "key=" << (tree_state_prefix + pso_tree_key);
    QSettings settings;
    settings.setValue(tree_state_prefix + pso_tree_key, expanded);
}

void TreeStateManager::save_query_subtree() {
    const QModelIndex queries_root = get_query_tree_root(console_);
    qInfo() << "[TreeStateManager] save_query_subtree: queries_root_valid=" << queries_root.isValid();
    if (!queries_root.isValid()) {
        qInfo() << "[TreeStateManager] save_query_subtree: aborted, invalid queries_root";
        return;
    }

    auto expanded_list = expanded_index_list(queries_root);
    QStringList query_path_list;
    const QStringList folder_list = settings_get_hash(SETTING_query_folders).keys();
    qInfo() << "[TreeStateManager] save_query_subtree: expanded_count=" << expanded_list.size()
            << "known_folder_count=" << folder_list.size();
    for (const QModelIndex &idx : expanded_list) {
        QString query_folder_path = console_query_folder_path(idx, console_);
        if (query_folder_path.isEmpty() || !folder_list.contains(query_folder_path)) {
            qInfo() << "[TreeStateManager] save_query_subtree: skipping path="
                    << query_folder_path << "(empty or not in known folder_list)";
            continue;
        }

        if (query_folder_path.startsWith("QUERY/")) {
            query_folder_path.remove(0, QString("QUERY/").length());
        }
        qInfo() << "[TreeStateManager] save_query_subtree: adding stripped path=" << query_folder_path;
        query_path_list.append(query_folder_path);
    }

    qInfo() << "[TreeStateManager] save_query_subtree: final path_count=" << query_path_list.size()
            << "paths=" << query_path_list << "key=" << (tree_state_prefix + queries_tree_key);
    QSettings settings;
    settings.setValue(tree_state_prefix + queries_tree_key, query_path_list);
}

QList<QModelIndex> TreeStateManager::expanded_index_list(const QModelIndex &parent) {
    QList<QModelIndex> out;
    const int row_count = source_model->rowCount(parent);

    for (int row = 0; row < row_count; ++row) {
        const QModelIndex idx = source_model->index(row, 0, parent);
        if (!idx.isValid()) {
            qInfo() << "[TreeStateManager] expanded_index_list: invalid index at row=" << row
                    << "skipping";
            continue;
        }

        const bool is_fetched = idx.data(ConsoleRole_WasFetched).toBool();
        const bool is_expanded = view_->isExpanded(proxy_model->mapFromSource(idx));
        const bool has_children = source_model->hasChildren(idx);
        if (is_fetched && is_expanded && has_children) {
            const QList<QModelIndex> child_expanded_idx_list = expanded_index_list(idx);
            if (!child_expanded_idx_list.isEmpty()) {
                qInfo() << "[TreeStateManager] expanded_index_list: row=" << row
                        << "appending" << child_expanded_idx_list.size() << "descendant(s) instead of self";
                out.append(child_expanded_idx_list);
            }
            else {
                qInfo() << "[TreeStateManager] expanded_index_list: row=" << row
                        << "appending self (leaf-of-expanded, no expanded descendants)";
                out.append(idx);
            }
        }
    }

    return out;
}

void TreeStateManager::save_current_selected_item() {
    SelectedItemData data = selected_item_data();
    qInfo() << "[TreeStateManager] save_current_selected_item: type=" << data.first
            << "name_data=" << data.second << "key=" << (tree_state_prefix + selected_item_key);
    QSettings settings;
    settings.setValue(tree_state_prefix + selected_item_key, QVariantList{data.first, data.second});
}

void TreeStateManager::restore_object_subtree() {
    const QModelIndex object_root = ConsoleObjectTreeOperations::get_domain_object_tree_root(console_);
    qInfo() << "[TreeStateManager] restore_object_subtree: object_root_valid=" << object_root.isValid();
    if (!object_root.isValid()) {
        qInfo() << "[TreeStateManager] restore_object_subtree: aborted, invalid object_root";
        return;
    }

    expand_dn_list(object_root, object_tree_key, ObjectRole_DN, ItemType_Object);
}

void TreeStateManager::restore_policy_subtree() {
    const QModelIndex policy_root_idx = get_policy_tree_root(console_);
    qInfo() << "[TreeStateManager] restore_policy_subtree: policy_root_valid=" << policy_root_idx.isValid();
    if (!policy_root_idx.isValid()) {
        qInfo() << "[TreeStateManager] restore_policy_subtree: aborted, invalid policy_root";
        return;
    }

    expand_dn_list(policy_root_idx, policy_tree_key, PolicyOURole_DN, ItemType_PolicyOU);

    // Check and expand All policies folder
    QSettings settings;
    // NOTE: missing tree_state_prefix here compared to save_policy_subtree() —
    // this reads a global, non-prefixed key instead of the per-domain/user one.
    qInfo() << "[TreeStateManager] restore_policy_subtree: reading all_policies_folder_key WITHOUT "
               "tree_state_prefix (key=" << all_policies_folder_key
            << ") -- likely mismatched with save_policy_subtree, which writes to "
            << (tree_state_prefix + policy_tree_key + all_policies_folder_key);
    const bool all_policies_folder_expanded = settings.value(tree_state_prefix + all_policies_folder_key).
                                              toBool();
    qInfo() << "[TreeStateManager] restore_policy_subtree: all_policies_folder_expanded="
            << all_policies_folder_expanded;
    if (all_policies_folder_expanded) {
        const QModelIndex all_policies_idx = get_all_policies_folder_index(console_);
        if (!all_policies_idx.isValid()) {
            qInfo() << "[TreeStateManager] restore_policy_subtree: aborted, "
                       "all_policies_idx invalid despite expanded flag";
            return;
        }

        view_->setExpanded(proxy_model->mapFromSource(all_policies_idx), true);
        qInfo() << "[TreeStateManager] restore_policy_subtree: all_policies_folder expanded=true";
    }
}

void TreeStateManager::restore_pso_subtree() {
    const QModelIndex pso_root = ConsoleObjectTreeOperations::get_pso_container_tree_root(console_);
    qInfo() << "[TreeStateManager] restore_pso_subtree: pso_root_valid=" << pso_root.isValid();
    if (!pso_root.isValid()) {
        qInfo() << "[TreeStateManager] restore_pso_subtree: aborted, invalid pso_root";
        return;
    }

    QSettings settings;
    const bool expanded = settings.value(tree_state_prefix + pso_tree_key).toBool();
    qInfo() << "[TreeStateManager] restore_pso_subtree: expanded="
            << expanded << "(NOTE: save_pso_subtree() is never called from save(), "
                           "so this key is likely always absent/false)";
    view_->setExpanded(proxy_model->mapFromSource(pso_root), expanded);
}

void TreeStateManager::restore_sites_subtree() {
    const QModelIndex sites_subtree_root = ConsoleObjectTreeOperations::get_sites_container_tree_root(console_);
    qInfo() << "[TreeStateManager] restore_sites_subtree: sites_root_valid=" << sites_subtree_root.isValid();
    if (!sites_subtree_root.isValid()) {
        qInfo() << "[TreeStateManager] restore_sites_subtree: aborted, invalid sites_subtree_root";
        return;
    }

    expand_dn_list(sites_subtree_root, sites_tree_key, ObjectRole_DN, ItemType_Object);
}

void TreeStateManager::restore_queries_subtree() {
    const QModelIndex queries_subtree_root = get_query_tree_root(console_);
    qInfo() << "[TreeStateManager] restore_queries_subtree: queries_root_valid="
            << queries_subtree_root.isValid();
    if (!queries_subtree_root.isValid()) {
        qInfo() << "[TreeStateManager] restore_queries_subtree: aborted, invalid queries_subtree_root";
        return;
    }

    QSettings settings;
    const QStringList path_list = settings.value(tree_state_prefix + queries_tree_key).toStringList();
    qInfo() << "[TreeStateManager] restore_queries_subtree: path_count=" << path_list.size()
            << "paths=" << path_list;
    if (path_list.isEmpty()) {
        qInfo() << "[TreeStateManager] restore_queries_subtree: aborted, empty path_list";
        return;
    }

    view_->setExpanded(proxy_model->mapFromSource(queries_subtree_root), true);

    for (auto path : path_list) {
        const QStringList path_names_list = path.split('/');
        for (auto query_folder_name : path_names_list) {
            // Queries and its folders are prefetched by default, so search them by name and expand
            const QModelIndex query_folder_index = console_->search_item(queries_subtree_root,
                Qt::DisplayRole, query_folder_name, {ItemType_QueryFolder});
            if (!query_folder_index.isValid()) {
                qInfo() << "[TreeStateManager] restore_queries_subtree: folder not found, name="
                        << query_folder_name << "(path=" << path << ")";
                continue;
            }

            view_->setExpanded(proxy_model->mapFromSource(query_folder_index), true);
            qInfo() << "[TreeStateManager] restore_queries_subtree: expanded folder name="
                    << query_folder_name;
        }
    }
}

void TreeStateManager::expand_dn_list(const QModelIndex &subtree_root, const QString &settings_key, int role, ItemType item_type) {
    QSettings settings;
    const QStringList dn_list_to_expand = settings.value(tree_state_prefix + settings_key);
    if (dn_list_to_expand.isEmpty()) {
        return;
    }

    view_->setExpanded(proxy_model->mapFromSource(subtree_root), true);

    QSet<QString> already_expanded;

    // Domain is fetched by default, so consider dn longer
    // then its dn (child objects)
    const int min_dn_parts_expand = 2;
    for (auto dn : dn_list_to_expand) {
        const QStringList dn_splitted = dn.split(',');
        if (dn_splitted.size() < min_dn_parts_expand) {
            continue;
        }

        QString current_expandable_dn = dn_splitted.last();
        for (int i = 0; i <= dn_splitted.size() - min_dn_parts_expand; ++i) {
            current_expandable_dn = dn_splitted[dn_splitted.size() - min_dn_parts_expand - i] + "," +
                                    current_expandable_dn;
            if (already_expanded.contains(current_expandable_dn)) {
                continue;
            }

            const QModelIndex idx = console_->search_item(subtree_root, role,
                current_expandable_dn, {item_type});
            if (idx.isValid()) {
                view_->setExpanded(proxy_model->mapFromSource(idx), true);
                already_expanded.insert(current_expandable_dn);
            }
        }
    }
}

void TreeStateManager::restore_current_selected_item() {
    QSettings settings;
    QVariant selected_item_variant = settings.value(tree_state_prefix + selected_item_key);
    if (selected_item_variant.isNull()) {
        return;
    }

    QVariantList selected_item_var_list = selected_item_variant.toList();
    if (selected_item_var_list.size() < 2) {
        return;
    }

    ItemType type = selected_item_var_list[0].toInt();
    const QString name_data = selected_item_var_list[1].toString();

    // NOTE: All corresponding subtrees should be fetched and expanded
    // before selected item selection restore
    switch (type) {
        case ItemType_Object:
            restore_object_selection(name_data);
            break;
        case ItemType_PolicyOU:
        case ItemType_Policy:
        {
            const int role = type == ItemType_PolicyOU ? PolicyOURole_DN : PolicyRole_DN;
            const QModelIndex policy_root = get_policy_tree_root(console_);
            const QModelIndex selected_policy_idx = console_->search_item(policy_root, role,
                name_data, {type});
            if (selected_policy_idx.isValid()) {
                console_->set_current_scope(selected_policy_idx);
                break;
            }
            break;
        }
        case ItemType_QueryFolder:
        {
            const QModelIndex queries_root = get_query_tree_root(console_);
            const QModelIndex selected_query_folder_idx = console_->search_item(queries_root, Qt::DisplayRole,
                name_data, {ItemType_QueryFolder});
            if (selected_query_folder_idx.isValid()) {
                console_->set_current_scope(selected_query_folder_idx);
                break;
            }
            break;
        }
        case ItemType_PolicyRoot:
        {
            const QModelIndex policy_root_idx = get_policy_tree_root(console_);
            if (policy_root_idx.isValid()) {
                console_->set_current_scope(policy_root_idx);
                break;
            }
            break;
        }
        case ItemType_AllPoliciesFolder:
        {
            const QModelIndex all_policies_idx = get_all_policies_folder_index(console_);
            if (all_policies_idx.isValid()) {
                console_->set_current_scope(all_policies_idx);
                break;
            }
            break;
        }
        default:
            console_->set_current_scope(console_->domain_info_index());
    }
}

void TreeStateManager::restore_object_selection(const QString &dn) {
    const bool is_pso = dn.contains(g_adconfig->pso_container_dn());
    if (is_pso) {
        const bool is_pso_container = dn == g_adconfig->pso_container_dn();
        const QModelIndex pso_subtree_root = ConsoleObjectTreeOperations::
            get_pso_container_tree_root(console_);
        if (pso_subtree_root.isValid() && is_pso_container) {
            console_->set_current_scope(pso_subtree_root);
            return;
        }

        const QModelIndex pso_idx = console_->search_item(pso_subtree_root, ObjectRole_DN,
            dn, {ItemType_Object});
        if (pso_idx.isValid()) {
            console_->set_current_scope(pso_idx);
            return;
        }
    }

    const bool is_site_obj = dn.contains(g_adconfig->sites_container_dn());
    if (is_site_obj) {
        const QModelIndex sites_root = ConsoleObjectTreeOperations::
            get_sites_container_tree_root(console_);
        const QModelIndex site_obj_idx = console_->search_item(sites_root, ObjectRole_DN,
            dn, {ItemType_Object});
        if (site_obj_idx.isValid()) {
            console_->set_current_scope(site_obj_idx);
            return;
        }
    }

    const bool is_domain_obj = dn.contains(g_adconfig->domain_dn());
    if (is_domain_obj) {
        const QModelIndex domain_obj_root = ConsoleObjectTreeOperations::
            get_domain_object_tree_root(console_);
        const QModelIndex domain_obj_idx = console_->search_item(domain_obj_root, ObjectRole_DN,
            dn, {ItemType_Object});
        if (domain_obj_idx.isValid()) {
            console_->set_current_scope(domain_obj_idx);
            return;
        }
    }
}

TreeStateManager::SelectedItemData TreeStateManager::selected_item_data() {
    auto selected_idx = console_->get_current_scope_item();
    if (!selected_idx.isValid()) {
        return SelectedItemData {ItemType_Unassigned, QString()};
    }

    const ItemType item_type = console_item_get_type(selected_idx);
    const QString name_data;
    switch (item_type) {
        case ItemType_Object:
            name_data = selected_idx.data(ObjectRole_DN).toString();
            break;
        case ItemType_PolicyOU:
            name_data = selected_idx.data(PolicyOURole_DN).toString();
            break;
        case ItemType_Policy:
            name_data = selected_idx.data(PolicyRole_DN).toString();
            break;
        case ItemType_QueryFolder:
            name_data = selected_idx.data(Qt::DisplayRole).toString();
            break;
        case ItemType_PolicyRoot:
        case ItemType_AllPoliciesFolder:
        default:
            break;
    }

    return SelectedItemData {static_cast<int>(item_type), name_data};
}
