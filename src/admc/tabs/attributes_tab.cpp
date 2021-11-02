/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2021 BaseALT Ltd.
 * Copyright (C) 2020-2021 Dmitry Degtyarev
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

#include "tabs/attributes_tab.h"
#include "tabs/ui_attributes_tab.h"

#include "adldap.h"
#include "editors/string_editor.h"
#include "editors/octet_editor.h"
#include "editors/bool_editor.h"
#include "editors/datetime_editor.h"
#include "editors/multi_editor.h"
#include "globals.h"
#include "settings.h"
#include "utils.h"
#include "tabs/attributes_tab_proxy.h"
#include "tabs/attributes_tab_filter_menu.h"

#include <QStandardItemModel>
#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QDebug>

QString attribute_type_display_string(const AttributeType type);

AttributesTab::AttributesTab() {
    ui = new Ui::AttributesTab();
    ui->setupUi(this);

    model = new QStandardItemModel(0, AttributesColumn_COUNT, this);
    set_horizontal_header_labels_from_map(model,
    {
        {AttributesColumn_Name, tr("Name")},
        {AttributesColumn_Value, tr("Value")},
        {AttributesColumn_Type, tr("Type")}
    });

    auto filter_menu = new AttributesTabFilterMenu(this);

    proxy = new AttributesTabProxy(filter_menu, this);

    proxy->setSourceModel(model);
    ui->view->setModel(proxy);

    ui->filter_button->setMenu(filter_menu);

    enable_widget_on_selection(ui->edit_button, ui->view);

    settings_restore_header_state(SETTING_attributes_tab_header_state, ui->view->header());

    const QHash<QString, QVariant> state = settings_get_variant(SETTING_attributes_tab_header_state).toHash();

    ui->view->header()->restoreState(state["header"].toByteArray());

    m_octet_editor = new OctetEditor(this);
    m_string_editor = new StringEditor(this);
    m_bool_editor = new BoolEditor(this);
    m_datetime_editor = new DateTimeEditor(this);
    m_multi_editor = new MultiEditor(this);

    const QList<AttributeEditor *> editor_list = {
        m_octet_editor,
        m_string_editor,
        m_bool_editor,
        m_datetime_editor,
        m_multi_editor,
    };

    for (AttributeEditor *editor : editor_list) {
        connect(
            editor, &AttributeEditor::accepted,
            this, &AttributesTab::on_editor_accepted);
    }

    connect(
        ui->view, &QAbstractItemView::doubleClicked,
        this, &AttributesTab::edit_attribute);
    connect(
        ui->edit_button, &QAbstractButton::clicked,
        this, &AttributesTab::edit_attribute);
    connect(
        filter_menu, &AttributesTabFilterMenu::filter_changed,
        proxy, &AttributesTabProxy::invalidate);
}

AttributesTab::~AttributesTab() {
    settings_set_variant(SETTING_attributes_tab_header_state, ui->view->header()->saveState());

    delete ui;
}

QList<QStandardItem *> AttributesTab::get_selected_row() const {
    const QItemSelectionModel *selection_model = ui->view->selectionModel();
    const QList<QModelIndex> selecteds = selection_model->selectedRows();

    if (selecteds.isEmpty()) {
        return QList<QStandardItem *>();
    }

    const QModelIndex proxy_index = selecteds[0];
    const QModelIndex index = proxy->mapToSource(proxy_index);

    const QList<QStandardItem *> row = [this, index]() {
        QList<QStandardItem *> out;
        for (int col = 0; col < AttributesColumn_COUNT; col++) {
            const QModelIndex item_index = index.siblingAtColumn(col);
            QStandardItem *item = model->itemFromIndex(item_index);
            out.append(item);
        }
        return out;
    }();

    return row;
}

void AttributesTab::edit_attribute() {
    const QList<QStandardItem *> row = get_selected_row();

    if (row.isEmpty()) {
        return;
    }

    const QString attribute = row[AttributesColumn_Name]->text();

    // Select an appropriate editor by attribute type and by
    // whether attribute is single or multi valued
    AttributeEditor *editor = [&]() -> AttributeEditor * {
        const bool single_valued = g_adconfig->get_attribute_is_single_valued(attribute);

        // Single/multi valued logic is separated out of the
        // switch statement for better flow
        AttributeEditor *octet_editor = [&]() -> AttributeEditor * {
            if (single_valued) {
                return m_octet_editor;
            } else {
                return m_multi_editor;
            }
        }();

        AttributeEditor *string_editor = [&]() -> AttributeEditor * {
            if (single_valued) {
                return m_string_editor;
            } else {
                return m_multi_editor;
            }
        }();

        AttributeEditor *bool_editor = [&]() -> AttributeEditor * {
            if (single_valued) {
                return m_bool_editor;
            } else {
                return m_multi_editor;
            }
        }();

        AttributeEditor *datetime_editor = [&]() -> AttributeEditor * {
            if (single_valued) {
                return m_datetime_editor;
            } else {
                return nullptr;
            }
        }();

        const AttributeType type = g_adconfig->get_attribute_type(attribute);

        switch (type) {
            case AttributeType_Octet: return octet_editor;
            case AttributeType_Sid: return octet_editor;

            case AttributeType_Boolean: return bool_editor;

            case AttributeType_Unicode: return string_editor;
            case AttributeType_StringCase: return string_editor;
            case AttributeType_DSDN: return string_editor;
            case AttributeType_IA5: return string_editor;
            case AttributeType_Teletex: return string_editor;
            case AttributeType_ObjectIdentifier: return string_editor;
            case AttributeType_Integer: return string_editor;
            case AttributeType_Enumeration: return string_editor;
            case AttributeType_LargeInteger: return string_editor;
            case AttributeType_UTCTime: return datetime_editor;
            case AttributeType_GeneralizedTime: return datetime_editor;
            case AttributeType_NTSecDesc: return string_editor;
            case AttributeType_Numeric: return string_editor;
            case AttributeType_Printable: return string_editor;
            case AttributeType_DNString: return string_editor;

            // NOTE: putting these here as confirmed to be unsupported
            case AttributeType_ReplicaLink: return nullptr;
            case AttributeType_DNBinary: return nullptr;
        }

        return nullptr;
    }();

    if (editor != nullptr) {
        editor->set_attribute(attribute);
        
        const QList<QByteArray> value_list = current[attribute];
        editor->set_value_list(value_list);

        editor->open();
    } else {
        message_box_critical(this, tr("Error"), tr("No editor is available for this attribute type."));
    }
}

void AttributesTab::load(AdInterface &ad, const AdObject &object) {
    UNUSED_ARG(ad);

    original.clear();

    for (auto attribute : object.attributes()) {
        original[attribute] = object.get_values(attribute);
    }

    // Add attributes without values
    const QList<QString> object_classes = object.get_strings(ATTRIBUTE_OBJECT_CLASS);
    const QList<QString> optional_attributes = g_adconfig->get_optional_attributes(object_classes);
    for (const QString &attribute : optional_attributes) {
        if (!original.contains(attribute)) {
            original[attribute] = QList<QByteArray>();
        }
    }

    current = original;

    proxy->load(object);

    model->removeRows(0, model->rowCount());

    for (auto attribute : original.keys()) {
        const QList<QStandardItem *> row = make_item_row(AttributesColumn_COUNT);
        const QList<QByteArray> values = original[attribute];

        model->appendRow(row);
        load_row(row, attribute, values);
    }
}

bool AttributesTab::apply(AdInterface &ad, const QString &target) {
    bool total_success = true;

    for (const QString &attribute : current.keys()) {
        const QList<QByteArray> current_values = current[attribute];
        const QList<QByteArray> original_values = original[attribute];

        if (current_values != original_values) {
            const bool success = ad.attribute_replace_values(target, attribute, current_values);
            if (success) {
                original[attribute] = current_values;
            } else {
                total_success = false;
            }
        }
    }

    return total_success;
}

void AttributesTab::load_row(const QList<QStandardItem *> &row, const QString &attribute, const QList<QByteArray> &values) {
    const QString display_values = attribute_display_values(attribute, values, g_adconfig);
    const AttributeType type = g_adconfig->get_attribute_type(attribute);
    const QString type_display = attribute_type_display_string(type);

    row[AttributesColumn_Name]->setText(attribute);
    row[AttributesColumn_Value]->setText(display_values);
    row[AttributesColumn_Type]->setText(type_display);
}

void AttributesTab::on_editor_accepted() {
    AttributeEditor *editor = qobject_cast<AttributeEditor *>(QObject::sender());

    const QList<QStandardItem *> row = get_selected_row();

    if (row.isEmpty()) {
        return;
    }
    
    const QString attribute = editor->get_attribute();
    const QList<QByteArray> value_list = editor->get_value_list();

    current[attribute] = value_list;
    load_row(row, attribute, value_list);

    emit edited();
}

QString attribute_type_display_string(const AttributeType type) {
    switch (type) {
        case AttributeType_Boolean: return AttributesTab::tr("Boolean");
        case AttributeType_Enumeration: return AttributesTab::tr("Enumeration");
        case AttributeType_Integer: return AttributesTab::tr("Integer");
        case AttributeType_LargeInteger: return AttributesTab::tr("Large Integer");
        case AttributeType_StringCase: return AttributesTab::tr("String Case");
        case AttributeType_IA5: return AttributesTab::tr("IA5");
        case AttributeType_NTSecDesc: return AttributesTab::tr("NT Security Descriptor");
        case AttributeType_Numeric: return AttributesTab::tr("Numeric");
        case AttributeType_ObjectIdentifier: return AttributesTab::tr("Object Identifier");
        case AttributeType_Octet: return AttributesTab::tr("Octet");
        case AttributeType_ReplicaLink: return AttributesTab::tr("Replica Link");
        case AttributeType_Printable: return AttributesTab::tr("Printable");
        case AttributeType_Sid: return AttributesTab::tr("SID");
        case AttributeType_Teletex: return AttributesTab::tr("Teletex");
        case AttributeType_Unicode: return AttributesTab::tr("Unicode");
        case AttributeType_UTCTime: return AttributesTab::tr("UTC Time");
        case AttributeType_GeneralizedTime: return AttributesTab::tr("Generalized Time");
        case AttributeType_DNString: return AttributesTab::tr("DN String");
        case AttributeType_DNBinary: return AttributesTab::tr("DN Binary");
        case AttributeType_DSDN: return AttributesTab::tr("Distinguished Name");
    }
    return QString();
}
