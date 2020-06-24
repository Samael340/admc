/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020 BaseALT Ltd.
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

#include "contents_widget.h"
#include "containers_widget.h"
#include "ad_interface.h"
#include "entry_proxy_model.h"
#include "entry_model.h"
#include "entry_context_menu.h"

#include <QTreeView>
#include <QLabel>
#include <QLayout>

ContentsWidget::ContentsWidget(EntryModel* model_arg, ContainersWidget *containers_widget, EntryContextMenu *entry_context_menu, QWidget *parent)
: EntryWidget(model_arg, parent)
{   
    model = model_arg;
    model->setHorizontalHeaderItem(Column::Name, new QStandardItem("Name"));
    model->setHorizontalHeaderItem(Column::Category, new QStandardItem("Category"));
    model->setHorizontalHeaderItem(Column::Description, new QStandardItem("Description"));
    model->setHorizontalHeaderItem(Column::DN, new QStandardItem("DN"));

    proxy = new EntryProxyModel(model, this);

    view->setAcceptDrops(true);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setRootIsDecorated(false);
    view->setItemsExpandable(false);
    view->setExpandsOnDoubleClick(false);
    view->setModel(proxy);
    entry_context_menu->connect_view(view, Column::DN);

    // Insert label into layout
    const auto label = new QLabel("Contents");
    layout()->removeWidget(view);
    layout()->addWidget(label);
    layout()->addWidget(view);

    column_hidden[Column::Name] = false;
    column_hidden[Column::Category] = false;
    column_hidden[Column::Description] = false;
    update_column_visibility();

    connect(
        containers_widget, &ContainersWidget::selected_changed,
        this, &ContentsWidget::change_target);

    connect(
        AD(), &AdInterface::create_entry_complete,
        this, &ContentsWidget::on_create_entry_complete);
    connect(
        AD(), &AdInterface::dn_changed,
        this, &ContentsWidget::on_dn_changed);
    connect(
        AD(), &AdInterface::attributes_changed,
        this, &ContentsWidget::on_attributes_changed);
}

void ContentsWidget::load_row(QList<QStandardItem *> row, const QString &dn) {
    // Load values into row
    QString name = AD()->get_attribute(dn, "name");

    // NOTE: this is given as raw DN and contains '-' where it should
    // have spaces, so convert it
    QString category = AD()->get_attribute(dn, "objectCategory");
    category = extract_name_from_dn(category);
    category = category.replace('-', ' ');

    QString description = AD()->get_attribute(dn, "description");

    row[Column::Name]->setText(name);
    row[Column::Category]->setText(category);
    row[Column::Description]->setText(description);

    QIcon icon = get_entry_icon(dn);
    row[0]->setIcon(icon);
}

void ContentsWidget::make_new_row(QStandardItem *parent, const QString &dn) {
    auto row = QList<QStandardItem *>();
    for (int i = 0; i < Column::COUNT; i++) {
        row.push_back(new QStandardItem());
    }

    load_row(row, dn);

    parent->appendRow(row);
}

void ContentsWidget::change_target(const QString &dn) {
    model->removeRows(0, model->rowCount());

    // Load head
    target_dn = dn;
    QStandardItem *root = model->invisibleRootItem();    
    make_new_row(root, target_dn);
    QStandardItem *head = model->item(0, 0);

    const QModelIndex head_index = head->index();
    const QModelIndex proxy_head_index = proxy->mapFromSource(head_index);
    view->setRootIndex(proxy_head_index);

    // Load children
    QList<QString> children = AD()->load_children(dn);
    for (auto child_dn : children) {
        make_new_row(head, child_dn);
    }

    update_column_visibility();
}

void ContentsWidget::remove_child(const QString &dn) {
    QList<QStandardItem *> row = model->find_row(dn);

    if (!row.isEmpty()) {
        const QStandardItem *item = row[0];
        item = row[2];
        QStandardItem *parent = item->parent();
        const int row_i = item->row();

        parent->removeRow(row_i);
    }
}

void ContentsWidget::on_dn_changed(const QString &old_dn, const QString &new_dn) {
    const bool deleted = (new_dn == "");

    if (deleted && old_dn == target_dn) {
        change_target("");

        return;
    }

    const QString new_parent = extract_parent_dn_from_dn(new_dn);
    const bool is_child = (old_dn != target_dn);
    if (is_child && new_parent != target_dn) {
        remove_child(old_dn);
    } else {
        // Update DN
        QStandardItem *dn_item = model->find_item(old_dn, Column::DN);

        if (dn_item != nullptr) {
            dn_item->setText(new_dn);
        }
    }
}

void ContentsWidget::on_create_entry_complete(const QString &dn, NewEntryType type) {
    QString parent_dn = extract_parent_dn_from_dn(dn);

    if (parent_dn == target_dn) {
        QStandardItem *head = model->item(0, 0);
        make_new_row(head, dn);
    }
}

void ContentsWidget::on_attributes_changed(const QString &dn) {
    QList<QStandardItem *> row = model->find_row(dn);

    if (!row.isEmpty()) {
        load_row(row, dn);
    }
}
