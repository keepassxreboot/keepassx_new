#  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

set(ykcore_SOURCES
    ykcore.c
    ykstatus.c
    ykcrc.c
)

add_library(ykcore STATIC ${ykcore_SOURCES})

if(WIN32)
    target_sources(ykcore PRIVATE ykcore_windows.c)
    target_link_libraries(ykcore PRIVATE uuid setupapi hid)
elseif(UNIX AND NOT APPLE)
    target_sources(ykcore PRIVATE ykcore_libusb-1.0.c)

    find_library(LIBUSB_LIBRARY NAMES usb-1.0)
    find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES "libusb-1.0" "libusb")
    if(NOT LIBUSB_LIBRARY OR NOT LIBUSB_INCLUDE_DIR)
        message(FATAL_ERROR "libusb-1.0 dev package required, but not found")
    endif()

    target_link_libraries(ykcore PRIVATE ${LIBUSB_LIBRARY})
    target_include_directories(ykcore PRIVATE ${LIBUSB_INCLUDE_DIR})
elseif(APPLE)
    target_sources(ykcore PRIVATE ykcore_osx.c)
    target_link_libraries(ykcore PUBLIC "-framework IOKit")
endif()
