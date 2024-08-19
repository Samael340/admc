#ifndef COMMON_PERMISSIONS_WIDGET_H
#define COMMON_PERMISSIONS_WIDGET_H

#include <QWidget>

#include "permissions_widget.h"

class CommonPermissionsWidget final : public PermissionsWidget {
    Q_OBJECT

public:
    explicit CommonPermissionsWidget(QWidget *parent = nullptr);
    ~CommonPermissionsWidget();

    virtual void init(const QStringList &target_classes,
                      security_descriptor *sd_arg) override;
};

#endif // COMMON_PERMISSIONS_WIDGET_H
