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

#include "panes.h"

#include "object_model.h"
#include "containers_proxy.h"
#include "advanced_view_proxy.h"
#include "object_menu.h"
#include "utils.h"
#include "ad_utils.h"
#include "details_dialog.h"
#include "ad_config.h"
#include "ad_interface.h"
#include "ad_object.h"
#include "filter.h"
#include "filter_dialog.h"
#include "filter_widget/filter_widget.h"
#include "object_menu.h"

#include <QTreeView>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDebug>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QApplication>
#include <QTreeWidget>

#define INVALID_ID -1

QHash<QString, int> dn_to_scope_id;
QHash<int, QStandardItemModel *> scope_id_to_results_model;

// NOTE: these don't include results from queries
QHash<QString, QStandardItem *> dn_to_scope_item;
QHash<QString, QStandardItem *> dn_to_results_item;

QString containers_filter();

Panes::Panes()
: QWidget()
{
    scope_model = new QStandardItemModel(0, 1, this);

    scope_view = new QTreeView(this);
    scope_view->setHeaderHidden(true);
    scope_view->setExpandsOnDoubleClick(true);
    scope_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    scope_view->setContextMenuPolicy(Qt::CustomContextMenu);
    // scope_view->setAcceptDrops(true); needed? setDragDropMode might be enough
    scope_view->setDragDropMode(QAbstractItemView::DragDrop);

    scope_view->setModel(scope_model);

    results_view = new QTreeView(this);
    results_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_view->header()->setSectionsMovable(true);
    results_view->setContextMenuPolicy(Qt::CustomContextMenu);

    auto splitter_a = new QSplitter(Qt::Vertical);
    auto test = new QTreeWidget();
    test->setHeaderHidden(true);
    new QTreeWidgetItem(test, {"test"});
    new QTreeWidgetItem(test, {"test 2"});
    splitter_a->addWidget(test);
    splitter_a->addWidget(scope_view);

    auto splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(splitter_a);
    splitter->addWidget(results_view);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
    layout->addWidget(splitter);

    filter_dialog = new FilterDialog(this);

    connect(
        filter_dialog, &QDialog::accepted,
        this, &Panes::reset);

    connect(
        scope_view->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &Panes::change_results_target);

    // TODO: not sure about this... Need to re-sort when items are renamed and maybe in other cases as well.
    connect(
        scope_model, &QAbstractItemModel::rowsInserted,
        [this]() {
            scope_view->sortByColumn(0, Qt::AscendingOrder);
        });

    connect(
        scope_view, &QTreeView::expanded,
        this, &Panes::fetch_scope_item);

    connect(
        qApp, &QApplication::focusChanged,
        this, &Panes::on_focus_changed);

    auto connect_context_menu =
    [this](QTreeView *view) {
        connect(
            view, &QWidget::customContextMenuRequested,
            [=](const QPoint pos) {
                open_context_menu(view, pos);
            });
    };
    connect_context_menu(scope_view);
    connect_context_menu(results_view);

    connect(
        AD(), &AdInterface::object_added,
        this, &Panes::on_object_added);
    connect(
        AD(), &AdInterface::object_deleted,
        this, &Panes::on_object_deleted);
    connect(
        AD(), &AdInterface::object_changed,
        this, &Panes::on_object_changed);
}

QModelIndex find_object(QStandardItemModel *model, const QString &dn) {
    for (int row = 0; row < model->rowCount(); row++) {
        QStandardItem *main_item = model->item(row, 0);
        const QString this_dn = main_item->data(Role_DN).toString();

        if (this_dn == dn) {
            return main_item->index();
        }
    }

    return QModelIndex();
}

void Panes::on_object_deleted(const QString &dn) {
    auto remove_row =
    [](QStandardItem *item) {
        if (item != nullptr) {
            QStandardItemModel *model = item->model();
            const QModelIndex index = item->index();

            model->removeRow(index.row(), index.parent());
        }
    };


    QStandardItem *scope_item = dn_to_scope_item.value(dn, nullptr);

    // Remove children of this scope item from internal data structures
    // NOTE: must do this before removing from model to be able to recurse through descendants
    if (scope_item != nullptr) {
        // auto remove_recursively =
        // [](QStandardItem *item) {
        //     // Remove this dn
        //     const QString this_dn = item->data(Role_DN).toString();
        //     dn_to_scope_id.remove(this_dn);
        //     dn_to_scope_item.remove(this_dn);

        //     const int this_id = item->data(Role_Id).toInt();
        //     QStandardItemModel *results_model = scope_id_to_results_model[this_id];
        //     delete results_model;
        //     scope_id_to_results_model.remove(this_id);

        //     // Remove children of this dn
        //     for (int row = 0; row < item->rowCount(); row++) {
        //         QStandardItem *child = item->child(0);

        //         // remove_recursively(child);
        //     }
        // };

        // remove_recursively(scope_item);
    }

    remove_row(scope_item);

    // Remove from results
    QStandardItem *results_item = dn_to_results_item.value(dn, nullptr);
    remove_row(results_item);
    dn_to_results_item.remove(dn);
}

void Panes::on_object_added(const QString &dn) {
    const AdObject object = AD()->search_object(dn);

    const QString parent_dn = dn_get_parent(dn);

    // Add to scope
    QStandardItem *scope_parent = dn_to_scope_item.value(parent_dn, nullptr);
    if (scope_parent != nullptr) {
        const bool fetched = scope_parent->data(Role_Fetched).toBool();

        // NOTE: only add if parent was fetched already. If parent wasn't fetched, then this new object will be added when parent is fetched.
        if (fetched) {
            make_scope_item(scope_parent, object);
        }
    }

    // Add to results
    const int scope_parent_id = dn_to_scope_id[parent_dn];
    QStandardItemModel *results_model = scope_id_to_results_model.value(scope_parent_id, nullptr);
    if (results_model != nullptr) {
        // TODO: duplicated in change_results_target()
        make_results_row(results_model, object);
    }
}

void Panes::make_results_row(QStandardItemModel * model, const AdObject &object) {
    const QList<QStandardItem *> row = make_item_row(ADCONFIG()->get_columns().size());
    load_results_row(row, object);
    dn_to_results_item[object.get_dn()] = row[0];
    model->appendRow(row);
}

void Panes::on_object_changed(const QString &dn) {
    // NOTE: attribute changes don't matter to scope because it doesn't display any attributes
    // So only need to update results
    // TODO: map dn's to result models they are in?
    const QString parent_dn = dn_get_parent(dn);

    if (!dn_to_scope_id.contains(parent_dn)) {
        return;
    }
    const int parent_id = dn_to_scope_id[parent_dn];

    if (!scope_id_to_results_model.contains(parent_id)) {
        return;
    }
    const QStandardItemModel *model = scope_id_to_results_model[parent_id];

    for (int row = 0; row < model->rowCount(); row++) {
        QStandardItem *main_item = model->item(row, 0);
        const QString this_dn = main_item->data(Role_DN).toString();

        if (this_dn == dn) {
            const QList<QStandardItem *> item_row =
            [=]() {
                QList<QStandardItem *> out;

                const QModelIndex main_index = main_item->index();

                for (int col = 0; col < ADCONFIG()->get_columns().size(); col++) {
                    const QModelIndex index = main_index.siblingAtColumn(col);
                    QStandardItem *item = model->itemFromIndex(index);

                    out.append(item);
                }

                return out;
            }();

            const AdObject object = AD()->search_object(dn);

            load_results_row(item_row, object);

            break;
        }
    }
}

void Panes::setup_menubar_menu(ObjectMenu *menu) {
    connect(
        menu, &QMenu::aboutToShow,
        [this, menu]() {
            menu->load_targets(focused_view);
        });
}

void Panes::open_context_menu(QTreeView *view, const QPoint pos) {
    auto menu = new ObjectMenu(focused_view);

    menu->load_targets(focused_view);

    if (!menu->targets.isEmpty()) {
        exec_menu_from_view(menu, focused_view, pos);
    }

    menu->deleteLater();
}

// NOTE: this is the workaround required to know in which pane selected objects are located
void Panes::on_focus_changed(QWidget *old, QWidget *now) {
    const QList<QTreeView *> views = {
        scope_view, results_view
    };
    for (auto view : views) {
        if (view == now) {
            focused_view = view;
            
            return;
        }
    }
}

void Panes::change_results_target(const QModelIndex &current, const QModelIndex &) {
    if (!current.isValid()) {
        return;
    }
    
    const int id = scope_model->data(current, Role_Id).toInt();

    // Load results for this scope item if needed
    if (!scope_id_to_results_model.contains(id)) {
        auto current_item = scope_model->itemFromIndex(current);

        auto model = new QStandardItemModel(0, 1, this);
        model->setHorizontalHeaderLabels(object_model_header_labels());

        const QString parent_dn = current_item->data(Role_DN).toString();

        // NOTE: don't apply filter from dialog to container objects by OR'ing that filter with one that accepts containers.
        const QString filter =
        [this]() {
            const QString filter_from_dialog = filter_dialog->filter_widget->get_filter();
            const QString containers = containers_filter();

            return filter_OR({filter_from_dialog, containers});
        }();
        
        const QList<QString> search_attributes = QList<QString>();
        const QHash<QString, AdObject> search_results = AD()->search(filter, search_attributes, SearchScope_Children, parent_dn);

        for (const AdObject object : search_results.values()) {
            make_results_row(model, object);
        }

        scope_id_to_results_model[id] = model;
    }

    QStandardItemModel *results_model = scope_id_to_results_model[id];

    results_view->setModel(results_model);

    // Fetch scope item as well for convenience
    fetch_scope_item(current);
}

void Panes::load_results_row(QList<QStandardItem *> row, const AdObject &object) {
    load_attributes_row(row, object);
    row[0]->setData(object.get_dn(), Role_DN);
    row[0]->setData(object.get_string(ATTRIBUTE_OBJECT_CLASS), Role_ObjectClass);
}

// Load scope item's children, if haven't done yet
void Panes::fetch_scope_item(const QModelIndex &index) {
    const bool fetched = index.data(Role_Fetched).toBool();
    if (fetched) {
        return;
    }

    show_busy_indicator();

    QStandardItem *item = scope_model->itemFromIndex(index);

    // NOTE: remove dummy child used for showing child indicator
    item->removeRow(0);

    const QString parent_dn = item->data(Role_DN).toString();

    // Add children
    QList<QString> search_attributes = ADCONFIG()->get_columns();
    
    const QString filter = containers_filter();
    
    QHash<QString, AdObject> search_results = AD()->search(filter, search_attributes, SearchScope_Children, parent_dn);

    for (const AdObject object : search_results.values()) {
        make_scope_item(item, object);
    }

    item->setData(true, Role_Fetched);

    hide_busy_indicator();
}

// TODO: should preserve states like expansion and current
// TODO: clear results
void Panes::reset() {
    scope_model->removeRows(0, scope_model->rowCount());

    // Load head object
    const QString head_dn = AD()->domain_head();
    const AdObject head_object = AD()->search_object(head_dn);
    make_scope_item(scope_model->invisibleRootItem(), head_object);

    // Make head object current
    scope_view->selectionModel()->setCurrentIndex(scope_model->index(0, 0), QItemSelectionModel::Current | QItemSelectionModel::ClearAndSelect);

    focused_view = scope_view;
}

void Panes::make_scope_item(QStandardItem *parent, const AdObject &object) {
    auto item = new QStandardItem();
    item->setData(false, Role_Fetched);

    // NOTE: add fake child to new items, so that the child indicator is shown while they are childless until they are fetched
    item->appendRow(new QStandardItem());
    
    const QString dn = object.get_dn();
    item->setData(dn, Role_DN);
    
    const QString name = dn_get_name(dn);
    item->setText(name);

    const QString object_class = object.get_string(ATTRIBUTE_OBJECT_CLASS);
    item->setData(object_class, Role_ObjectClass);

    static int id_max = 0;
    const int id = id_max;
    id_max++;
    item->setData(id, Role_Id);

    dn_to_scope_id[dn] = id;
    // TODO: unmap this when items are destroyed (object deleted, parent deleted, etc)
    dn_to_scope_item[dn] = item;

    const QIcon icon = object.get_icon();
    item->setIcon(icon);

    parent->appendRow(item);
}

QString containers_filter() {
    const QList<QString> accepted_classes = ADCONFIG()->get_filter_containers();

    QList<QString> class_filters;
    for (const QString object_class : accepted_classes) {
        const QString class_filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, object_class);
        class_filters.append(class_filter);
    }

    return filter_OR(class_filters);
};