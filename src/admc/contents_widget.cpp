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
#include "advanced_view_proxy.h"
#include "object_context_menu.h"
#include "details_widget.h"
#include "settings.h"
#include "utils.h"

#include <QTreeView>
#include <QLabel>
#include <QVBoxLayout>

enum ContentsColumn {
    ContentsColumn_Name,
    ContentsColumn_Category,
    ContentsColumn_Description,
    ContentsColumn_DN,
    ContentsColumn_COUNT,
};

ContentsWidget::ContentsWidget(ContainersWidget *containers_widget, QWidget *parent)
: QWidget(parent)
{   
    model = new ContentsModel(this);
    const auto advanced_view_proxy = new AdvancedViewProxy(ContentsColumn_DN, this);

    view = new QTreeView(this);
    view->setAcceptDrops(true);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setRootIsDecorated(false);
    view->setItemsExpandable(false);
    view->setExpandsOnDoubleClick(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setDragDropMode(QAbstractItemView::DragDrop);
    view->setAllColumnsShowFocus(true);
    view->setSortingEnabled(true);
    ObjectContextMenu::connect_view(view, ContentsColumn_DN);

    setup_model_chain(view, model, {advanced_view_proxy});

    setup_column_toggle_menu(view, model, {ContentsColumn_Name, ContentsColumn_Category, ContentsColumn_Description});

    label = new QLabel(this);

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(label);
    layout->addWidget(view);

    connect(
        containers_widget, &ContainersWidget::selected_changed,
        this, &ContentsWidget::on_containers_selected_changed);

    connect(
        AdInterface::instance(), &AdInterface::modified,
        this, &ContentsWidget::on_ad_modified);

    connect(
        view, &QAbstractItemView::clicked,
        this, &ContentsWidget::on_view_clicked);
}

void ContentsWidget::on_containers_selected_changed(const QString &dn) {
    change_target(dn);
}

void ContentsWidget::on_ad_modified() {
    change_target(target_dn);
}

void ContentsWidget::on_view_clicked(const QModelIndex &index) {
    const bool details_from_contents = Settings::instance()->get_bool(BoolSetting_DetailsFromContents);

    if (details_from_contents) {
        const QString dn = get_dn_from_index(index, ContentsColumn_DN);
        DetailsWidget::change_target(dn);
        printf("dn=%s\n", qPrintable(dn));
    }
}

void ContentsWidget::change_target(const QString &dn) {
    target_dn = dn;

    model->change_target(target_dn);
    
    const QAbstractItemModel *view_model = view->model();
    
    // Set root to head
    // NOTE: need this to hide head while retaining it in model for drag and drop purposes
    const QModelIndex head_index = view_model->index(0, 0);
    view->setRootIndex(head_index);

    resize_columns();

    const QString target_name = AdInterface::instance()->attribute_get(target_dn, ATTRIBUTE_NAME);

    QString label_text;
    if (target_name.isEmpty()) {
        label_text = "";
    } else {
        const QModelIndex view_head = view_model->index(0, 0);
        const int object_count = view_model->rowCount(view_head);

        const QString objects_string = tr("%n object(s)", "", object_count);
        label_text = QString("%1: %2").arg(target_name, objects_string);
    }
    label->setText(label_text);
}

void ContentsWidget::resize_columns() {
    const int view_width = view->width();
    const int name_width = (int) (view_width * 0.4);
    const int category_width = (int) (view_width * 0.15);
    view->setColumnWidth(ContentsColumn_Name, name_width);
    view->setColumnWidth(ContentsColumn_Category, category_width);
}

void ContentsWidget::showEvent(QShowEvent *event) {
    resize_columns();
}

ContentsModel::ContentsModel(QObject *parent)
: ObjectModel(ContentsColumn_COUNT, ContentsColumn_DN, parent)
{
    set_horizontal_header_labels_from_map(this, {
        {ContentsColumn_Name, tr("Name")},
        {ContentsColumn_Category, tr("Category")},
        {ContentsColumn_Description, tr("Description")},
        {ContentsColumn_DN, tr("DN")}
    });
}

void ContentsModel::change_target(const QString &target_dn) {
    removeRows(0, rowCount());

    if (target_dn == "") {
        return;
    }

    // Load head
    QStandardItem *root = invisibleRootItem();    
    make_row(root, target_dn);
    QStandardItem *head = item(0, 0);

    // Load children
    QList<QString> children = AdInterface::instance()->list(target_dn);
    for (auto child_dn : children) {
        make_row(head, child_dn);
    }
}

void ContentsModel::make_row(QStandardItem *parent, const QString &dn) {
    const QList<QStandardItem *> row = make_item_row(ContentsColumn_COUNT);

    const QString name = AdInterface::instance()->attribute_get(dn, ATTRIBUTE_NAME);

    // NOTE: this is given as raw DN and contains '-' where it should
    // have spaces, so convert it
    QString category = AdInterface::instance()->attribute_get(dn, ATTRIBUTE_OBJECT_CATEGORY);
    category = extract_name_from_dn(category);
    category = category.replace('-', ' ');

    const QString description = AdInterface::instance()->attribute_get(dn, ATTRIBUTE_DESCRIPTION);

    row[ContentsColumn_Name]->setText(name);
    row[ContentsColumn_Category]->setText(category);
    row[ContentsColumn_Description]->setText(description);
    row[ContentsColumn_DN]->setText(dn);

    const QIcon icon = get_object_icon(dn);
    row[0]->setIcon(icon);

    parent->appendRow(row);
}
