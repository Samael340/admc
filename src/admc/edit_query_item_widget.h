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

#ifndef EDIT_QUERY_ITEM_WIDGET_H
#define EDIT_QUERY_ITEM_WIDGET_H

/**
 * Widget used for editing queries. Used in edit query
 * dialog and create query dialog.
 */

#include <QModelIndex>
#include <QString>
#include <QWidget>

class EditQueryItemFilterDialog;
class QLineEdit;
class QTextEdit;
class SelectBaseWidget;
class QCheckBox;

class EditQueryItemWidget : public QWidget {
    Q_OBJECT

public:
    EditQueryItemWidget();

    void load(const QModelIndex &index);
    void save(QString &name, QString &description, QString &filter, QString &base, bool &scope_is_children, QByteArray &filter_state) const;

private:
    EditQueryItemFilterDialog *dialog;
    QLineEdit *name_edit;
    QLineEdit *description_edit;
    QTextEdit *filter_display;
    SelectBaseWidget *select_base_widget;
    QCheckBox *scope_checkbox;

    void update_filter_display();
};

#endif /* EDIT_QUERY_ITEM_WIDGET_H */
