/*
 * GPGUI - Group Policy Editor GUI
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

#include "config.h"

#include <memory>

#include "Runner.h"

int main(int argc, char **argv) {
    std::unique_ptr<Runner> app(new Runner(
        argc, argv, GPGUI_APPLICATION_DISPLAY_NAME, GPGUI_APPLICATION_NAME,
        GPGUI_VERSION, GPGUI_ORGANIZATION, GPGUI_ORGANIZATION_DOMAIN));
    return app->run();
}