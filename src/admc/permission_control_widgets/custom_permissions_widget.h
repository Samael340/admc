#ifndef CUSTOMPERMISSIONSWIDGET_H
#define CUSTOMPERMISSIONSWIDGET_H

#include "permissions_widget.h"

class CustomPermissionsWidget final : public PermissionsWidget {

    Q_OBJECT
public:
    CustomPermissionsWidget(QWidget *parent = nullptr);
    ~CustomPermissionsWidget();

    virtual void init(const QStringList &target_classes,
                      security_descriptor *sd_arg) override;

private:
    virtual void on_item_changed(QStandardItem *item) override;
    virtual void update_model_right_items(const QModelIndex &index) override;
};

#endif // CUSTOMPERMISSIONSWIDGET_H
