#  Copyright (C) 2019 KeePassXC Team
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

set(cli_SOURCES
        Add.cpp
        AddGroup.cpp
        Analyze.cpp
        AttachmentExport.cpp
        AttachmentImport.cpp
        AttachmentRemove.cpp
        Clip.cpp
        Close.cpp
        Create.cpp
        Command.cpp
        DatabaseCommand.cpp
        Diceware.cpp
        Edit.cpp
        Estimate.cpp
        Exit.cpp
        Export.cpp
        Generate.cpp
        Help.cpp
        Import.cpp
        Info.cpp
        List.cpp
        Locate.cpp
        Merge.cpp
        Move.cpp
        Open.cpp
        Remove.cpp
        RemoveGroup.cpp
        Show.cpp)

add_library(cli STATIC ${cli_SOURCES})
target_link_libraries(cli Qt5::Core Qt5::Widgets)

find_package(Readline)

if (READLINE_FOUND)
    target_compile_definitions(cli PUBLIC USE_READLINE)
    target_link_libraries(cli readline)
endif()

add_executable(keepassxc-cli keepassxc-cli.cpp)
target_link_libraries(keepassxc-cli
        ${GPGERROR_LIBRARIES}
        cli
        keepassx_core
        Qt5::Core
        ${GCRYPT_LIBRARIES}
        ${sodium_LIBRARY_RELEASE}
        ${ARGON2_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${ZXCVBN_LIBRARIES})

install(TARGETS keepassxc-cli
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION ${CLI_INSTALL_DIR} COMPONENT Runtime)

if(MINGW)
    install(CODE "include(BundleUtilities)
                  fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/keepassxc-cli.exe\" \"\" \"\")"
            COMPONENT Runtime)
endif()

if(APPLE AND WITH_APP_BUNDLE)
    add_custom_command(TARGET keepassxc-cli
                       POST_BUILD
                       COMMAND ${CMAKE_INSTALL_NAME_TOOL}
                            -change /usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore
                                "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore"
                            -change /usr/local/opt/qt/lib/QtGui.framework/Versions/5/QtGui
                                "@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui"
                            -change /usr/local/opt/qt/lib/QtMacExtras.framework/Versions/5/QtMacExtras
                                "@executable_path/../Frameworks/QtMacExtras.framework/Versions/5/QtMacExtras"
                            -change /usr/local/opt/qt/lib/QtConcurrent.framework/Versions/5/QtConcurrent
                                "@executable_path/../Frameworks/QtConcurrent.framework/Versions/5/QtConcurrent"
                            -change /usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore
                                "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore"
                            -change /usr/local/opt/qt/lib/QtNetwork.framework/Versions/5/QtNetwork
                                "@executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork"
                            -change /usr/local/opt/qt/lib/QtWidgets.framework/Versions/5/QtWidgets
                                "@executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets"
                            -change /usr/local/opt/qt/lib/QtSvg.framework/Versions/5/QtSvg
                                "@executable_path/../Frameworks/QtSvg.framework/Versions/5/QtSvg"
                            -change /usr/local/opt/libgcrypt/lib/libgcrypt.20.dylib
                                "@executable_path/../Frameworks/libgcrypt.20.dylib"
                            -change /usr/local/opt/argon2/lib/libargon2.1.dylib
                                "@executable_path/../Frameworks/libargon2.1.dylib"
                            -change /usr/local/opt/libgpg-error/lib/libgpg-error.0.dylib
                                "@executable_path/../Frameworks/libgpg-error.0.dylib"
                            -change /usr/local/opt/libsodium/lib/libsodium.23.dylib
                                "@executable_path/../Frameworks/libsodium.23.dylib"
                            -change /usr/local/opt/qrencode/lib/libqrencode.4.dylib
                                "@executable_path/../Frameworks/libqrencode.4.dylib"
                            -change /usr/local/opt/libyubikey/lib/libyubikey.0.dylib
                                "@executable_path/../Frameworks/libyubikey.0.dylib"
                            -change /usr/local/opt/ykpers/lib/libykpers-1.1.dylib
                                "@executable_path/../Frameworks/libykpers-1.1.dylib"
                            -change /usr/local/opt/quazip/lib/libquazip.1.dylib
                                "@executable_path/../Frameworks/libquazip.1.dylib"
                       keepassxc-cli
                       COMMENT "Changing linking of keepassxc-cli")

    # Copy app to staging directory for pre-install testing
    set(CLI_APP_DIR "${CMAKE_BINARY_DIR}/src/${CLI_INSTALL_DIR}")
    add_custom_command(TARGET keepassxc-cli
                       POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy keepassxc-cli ${CLI_APP_DIR}/keepassxc-cli
                       COMMENT "Copying keepassxc-cli inside the application")
endif()
