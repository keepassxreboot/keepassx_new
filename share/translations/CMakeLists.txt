#  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
#  Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 or (at your option)
#  version 3 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

file(GLOB TRANSLATION_FILES *.ts)
get_filename_component(TRANSLATION_EN_ABS keepassx_en.ts ABSOLUTE)
list(REMOVE_ITEM TRANSLATION_FILES keepassx_en.ts)
list(REMOVE_ITEM TRANSLATION_FILES ${TRANSLATION_EN_ABS})
message(STATUS "Including translations...\n")

qt5_add_translation(QM_FILES ${TRANSLATION_FILES})

if(MINGW)
    file(GLOB QTBASE_TRANSLATIONS ${Qt5_PREFIX}/share/qt5/translations/qtbase_*.qm)
elseif(MSVC)
    file(GLOB QTBASE_TRANSLATIONS ${Qt5_PREFIX}/translations/qtbase_*.qm)
elseif(APPLE OR KEEPASSXC_DIST_APPIMAGE)
    file(GLOB QTBASE_TRANSLATIONS
            /usr/share/qt/translations/qtbase_*.qm
            /usr/share/qt5/translations/qtbase_*.qm
            ${Qt5_PREFIX}/translations/qtbase_*.qm)
endif()
set(QM_FILES ${QM_FILES} ${QTBASE_TRANSLATIONS})

install(FILES ${QM_FILES} DESTINATION ${DATA_INSTALL_DIR}/translations)

# Add keepassx_en.qm as a fallback for uncommon english locales
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/keepassx_en_US.qm DESTINATION ${DATA_INSTALL_DIR}/translations RENAME keepassx_en.qm)

add_custom_target(translations DEPENDS ${QM_FILES})
add_dependencies(${PROGNAME} translations)
