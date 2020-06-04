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

#if !defined(__ADMC_APPLICATION_H)
#define __ADMC_APPLICATION_H 1

#include "ad_connection.h"

#include <QApplication>

class ADMC final: public QApplication {
    Q_OBJECT

    adldap::AdConnection* connection;

public:
    ADMC(int& argc, char** argv);
    adldap::AdConnection* get_connection();
};

#endif /* __ADMC_APPLICATION_H */
