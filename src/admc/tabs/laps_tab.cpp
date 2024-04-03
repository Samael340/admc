/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2022 BaseALT Ltd.
 * Copyright (C) 2020-2022 Dmitry Degtyarev
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

#include "tabs/laps_tab.h"
#include "tabs/ui_laps_tab.h"

#include "adldap.h"
#include "attribute_edits/laps_expiry_edit.h"
#include "attribute_edits/string_edit.h"

LAPSTab::LAPSTab(QList<AttributeEdit *> *edit_list, bool is_legacy, QWidget *parent)
: QWidget(parent) {
    ui = new Ui::LAPSTab();
    ui->setupUi(this);

    QString password_attr = is_legacy ? ATTRIBUTE_LEGACY_LAPS_PASSWORD : ATTRIBUTE_LAPS_PASSWORD;
    QString expiry_attr = is_legacy ? ATTRIBUTE_LEGACY_LAPS_EXPIRATION : ATTRIBUTE_LAPS_EXPIRATION;

    auto pass_edit = new StringEdit(ui->pass_lineedit, password_attr, this);
    auto laps_edit = new LAPSExpiryEdit(ui->expiry_datetimeedit, ui->reset_expiry_button, expiry_attr, this);

    edit_list->append({
        pass_edit,
        laps_edit,
    });
}

LAPSTab::~LAPSTab() {
    delete ui;
}
