/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2024 BaseALT Ltd.
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

#ifndef EXTENDEDPERMISSIONSWIDGET_H
#define EXTENDEDPERMISSIONSWIDGET_H

#include "permission_control_widgets/permissions_widget.h"


struct SecurityRight;

class ExtendedPermissionsWidget final : public PermissionsWidget {
    Q_OBJECT

public:
    enum ExtRightsItemType {
        ExtRightsItemType_ExtRight,
        ExtRightsItemType_CommonTask
    };

    // NOTE: Each common task can represent combination of different
    // rights, including common rights.
    // Tasks are not equal extended rights with similar name.
    enum CommonTask {
        CommonTask_UserControl, // User creation, deletion and control
        CommonTask_ChangeUserPassword, // Reset user password and force password change at next logon
        CommonTask_ReadAllUserInformation,
        CommonTask_GroupControl, // Group creation, deletion and control
        CommonTask_InetOrgPersonControl, // InetOrgPerson creation, deletion and control
        CommonTask_ChangeInetOrgPersonPassword, // Reset user password and force password change at next logon
        CommonTask_ReadAllInetOrgPersonInformation,
        CommonTask_GroupMembership,
        CommonTask_ManageGPLinks,

        CommonTask_COUNT
    };

    ExtendedPermissionsWidget(QWidget *parent = nullptr);
    ~ExtendedPermissionsWidget();

    virtual void init(const QStringList &target_classes,
                      security_descriptor *sd_arg) override;

private:
    virtual void update_model_right_items(const QModelIndex &parent) override;
    virtual void make_model_rights_read_only(const QModelIndex &parent) override;
    virtual void on_item_changed(QStandardItem *item) override;

    QList<QStandardItem*> make_right_item_row(const QString &name, const QList<SecurityRight> &right);
    QList<QStandardItem*> create_title_row(const QString &text, ExtRightsItemType type);
    void append_extended_right_items();
    void load_common_tasks();
    void load_common_tasks_rights();
    void append_common_task_items();

    QHash<CommonTask, QList<SecurityRight>> common_task_rights;
    QHash<CommonTask, QString> common_task_name;
    QHash<QString, QList<CommonTask>> class_common_task_rights_map;
};

#endif // EXTENDEDPERMISSIONSWIDGET_H
