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

#ifndef TREE_STATE_MANAGER_H
#define TREE_STATE_MANAGER_H

#include <QString>
#include <QPair>
#include "core/console_item_type.h"

class ConsoleWidget;
class QModelIndex;
class QTreeView;
class QAbstractItemModel;
class QAbstractProxyModel;

class TreeStateManager final {
    using SelectedItemData = QPair<ItemType, QString>;

public:
    explicit TreeStateManager(ConsoleWidget *console, QTreeView *view, const QString &domain, const QString &user);

    void set_context(const QString &domain, const QString &user);
    void save();
    void restore();
    // Used to restore subtree after refreshing
    void restore_subtree(const QModelIndex &parent);

private:
    ConsoleWidget *console_;
    QString domain_;
    QString user_;
    QTreeView *view_;
    QAbstractProxyModel *proxy_model;
    QAbstractItemModel *source_model;

    // Prefix and keys for tree state settings
    QString tree_state_prefix;
    const QString object_tree_key = "objects";
    const QString policy_tree_key = "policies";
    const QString all_policies_folder_key = "all_policies";
    const QString pso_tree_key = "pso";
    const QString sites_tree_key = "sites";
    const QString queries_tree_key = "queries";
    const QString selected_item_key = "selected_item";

    void save_object_subtree(const QModelIndex &parent, const QString &key);
    void save_policy_subtree();
    void save_pso_subtree();
    void save_query_subtree();
    QList<QModelIndex> expanded_index_list(const QModelIndex &parent);

    void save_current_selected_item();

    void restore_object_subtree();
    void restore_policy_subtree();
    void restore_pso_subtree();
    void restore_sites_subtree();
    void restore_queries_subtree();

    void expand_dn_list(const QModelIndex &subtree_root, const QString &key, int role, ItemType item_type);

    void restore_current_selected_item();
    void restore_object_selection(const QString &dn);

    // Returns item type and DN (for AD objects) or item name
    // (for queries). For "All policies" folder and
    SelectedItemData selected_item_data();
};

#endif // TREE_STATE_MANAGER_H
