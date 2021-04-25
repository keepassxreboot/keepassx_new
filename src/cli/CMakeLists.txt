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
target_link_libraries(cli Qt5::Core)

find_package(Readline)

if (READLINE_FOUND)
    target_compile_definitions(cli PUBLIC USE_READLINE)
    target_link_libraries(cli readline)
endif()

add_executable(keepassxc-cli keepassxc-cli.cpp)
target_link_libraries(keepassxc-cli
        ${GPGERROR_LIBRARIES}
        cli
        keepassx_core)

install(TARGETS keepassxc-cli
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION ${CLI_INSTALL_DIR} COMPONENT Runtime)

if(MINGW)
    install(CODE "include(BundleUtilities)
                  fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/keepassxc-cli.exe\" \"\" \"\")"
            COMPONENT Runtime)
endif()

if(APPLE AND WITH_APP_BUNDLE)
    set(CLI_APP_DIR "${CMAKE_BINARY_DIR}/src/${CLI_INSTALL_DIR}")
    add_custom_command(TARGET keepassxc-cli
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/keepassxc-cli ${CLI_APP_DIR}/keepassxc-cli
            COMMAND ${MACDEPLOYQT_EXE} ${PROGNAME}.app -executable=${CLI_APP_DIR}/keepassxc-cli -no-plugins 2> /dev/null
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/src
            COMMENT "Deploying keepassxc-cli")
endif()
