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

#ifndef CREATE_OBJECT_DIALOG_H
#define CREATE_OBJECT_DIALOG_H

/**
 * Base class for dialogs that create objects. 
 */

#include <QDialog>

class QLineEdit;
class QDialogButtonBox;
class AttributeEdit;

class CreateObjectDialog : public QDialog {
    Q_OBJECT

public:
    using QDialog::QDialog;

    virtual QString get_created_dn() const = 0;
};

class CreateObjectHelper : public QObject {
    Q_OBJECT

public:
    CreateObjectHelper(QLineEdit *name_edit, QDialogButtonBox *button_box, const QList<AttributeEdit *> &edits_list, const QList<QLineEdit *> &required_list, const QString &object_class, const QString &parent_dn, QDialog *parent_dialog);

    static void success_msg(const QString &old_name);
    static void fail_msg(const QString &old_name);

    bool accept() const;
    void on_edited();
    QString get_created_name() const;
    QString get_created_dn() const;

private:
    QDialog *parent_dialog;
    QString parent_dn;
    QLineEdit *name_edit;
    QList<AttributeEdit *> m_edit_list;
    QList<QLineEdit *> m_required_list;
    QPushButton *ok_button;
    QString m_object_class;
};

#endif /* CREATE_OBJECT_DIALOG_H */
