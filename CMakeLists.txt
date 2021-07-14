#  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
#  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

cmake_minimum_required(VERSION 3.3.0)

project(KeePassXC)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
            "Choose the type of build, options are: Debug Release RelWithDebInfo Profile"
            FORCE)
endif()
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Support Visual Studio Code
include(CMakeToolsHelpers OPTIONAL)
include(FeatureSummary)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

option(WITH_TESTS "Enable building of unit tests" ON)
option(WITH_GUI_TESTS "Enable building of GUI tests" OFF)
option(WITH_DEV_BUILD "Use only for development. Disables/warns about deprecated methods." OFF)
option(WITH_ASAN "Enable address sanitizer checks (Linux / macOS only)" OFF)
option(WITH_COVERAGE "Use to build with coverage tests (GCC only)." OFF)
option(WITH_APP_BUNDLE "Enable Application Bundle for macOS" ON)
option(WITH_CCACHE "Use ccache for build" OFF)

set(WITH_XC_ALL OFF CACHE BOOL "Build in all available plugins")

option(WITH_XC_AUTOTYPE "Include Auto-Type." ON)
option(WITH_XC_NETWORKING "Include networking code (e.g. for downloading website icons)." OFF)
option(WITH_XC_BROWSER "Include browser integration with keepassxc-browser." OFF)
option(WITH_XC_YUBIKEY "Include YubiKey support." OFF)
option(WITH_XC_SSHAGENT "Include SSH agent support." OFF)
option(WITH_XC_KEESHARE "Sharing integration with KeeShare (requires quazip5 for secure containers)" OFF)
option(WITH_XC_UPDATECHECK "Include automatic update checks; disable for controlled distributions" ON)
if(UNIX AND NOT APPLE)
    option(WITH_XC_FDOSECRETS "Implement freedesktop.org Secret Storage Spec server side API." OFF)
endif()
if(APPLE)
    option(WITH_XC_TOUCHID "Include TouchID support for macOS." OFF)
endif()
option(WITH_XC_DOCS "Enable building of documentation" ON)

if(WITH_CCACHE)
    # Use the Compiler Cache (ccache) program
    # (install with: sudo apt get ccache)
    find_program(CCACHE_FOUND ccache)
    if(NOT CCACHE_FOUND)
        message(FATAL_ERROR "ccache requested but cannot be found.")
    endif()
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_FOUND})
endif()

if(WITH_XC_ALL)
    # Enable all options (except update check and docs)
    set(WITH_XC_AUTOTYPE ON)
    set(WITH_XC_NETWORKING ON)
    set(WITH_XC_BROWSER ON)
    set(WITH_XC_YUBIKEY ON)
    set(WITH_XC_SSHAGENT ON)
    set(WITH_XC_KEESHARE ON)
    if(APPLE)
        set(WITH_XC_TOUCHID ON)
    endif()
    if(UNIX AND NOT APPLE)
        set(WITH_XC_FDOSECRETS ON)
    endif()
endif()

# Prefer WITH_XC_NETWORKING setting over WITH_XC_UPDATECHECK
if(NOT WITH_XC_NETWORKING AND WITH_XC_UPDATECHECK)
    message(STATUS "Disabling WITH_XC_UPDATECHECK because WITH_XC_NETWORKING is disabled")
    set(WITH_XC_UPDATECHECK OFF)
endif()

set(KEEPASSXC_VERSION_MAJOR "2")
set(KEEPASSXC_VERSION_MINOR "7")
set(KEEPASSXC_VERSION_PATCH "0")
set(KEEPASSXC_VERSION "${KEEPASSXC_VERSION_MAJOR}.${KEEPASSXC_VERSION_MINOR}.${KEEPASSXC_VERSION_PATCH}")
set(OVERRIDE_VERSION "" CACHE STRING "Override the KeePassXC Version for Snapshot builds")

set(KEEPASSXC_BUILD_TYPE "Snapshot" CACHE STRING "Set KeePassXC build type to distinguish between stable releases and snapshots")
set_property(CACHE KEEPASSXC_BUILD_TYPE PROPERTY STRINGS Snapshot Release PreRelease)

# Retrieve git HEAD revision hash
set(GIT_HEAD_OVERRIDE "" CACHE STRING "Manually set the Git HEAD hash when missing (eg, when no .git folder exists)")
execute_process(COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HEAD
        ERROR_QUIET)
string(STRIP "${GIT_HEAD}" GIT_HEAD)
if(GIT_HEAD STREQUAL "" AND NOT GIT_HEAD_OVERRIDE STREQUAL "")
    string(SUBSTRING "${GIT_HEAD_OVERRIDE}" 0 7 GIT_HEAD)
elseif(EXISTS ${CMAKE_SOURCE_DIR}/.gitrev)
    file(READ ${CMAKE_SOURCE_DIR}/.gitrev GIT_HEAD)
endif()
message(STATUS "Found Git HEAD Revision: ${GIT_HEAD}\n")

# Check if on a tag, if so build as a release
execute_process(COMMAND git tag --points-at HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        ERROR_QUIET)
if(GIT_TAG)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    set(OVERRIDE_VERSION ${GIT_TAG})
elseif(EXISTS ${CMAKE_SOURCE_DIR}/.version)
    file(READ ${CMAKE_SOURCE_DIR}/.version OVERRIDE_VERSION)
endif()

string(REGEX REPLACE "(\r?\n)+" "" OVERRIDE_VERSION "${OVERRIDE_VERSION}")
if(OVERRIDE_VERSION)
    if(OVERRIDE_VERSION MATCHES "^[\\.0-9]+-(alpha|beta)[0-9]+$")
        set(KEEPASSXC_BUILD_TYPE PreRelease)
        set(KEEPASSXC_VERSION ${OVERRIDE_VERSION})
    elseif(OVERRIDE_VERSION MATCHES "^[\\.0-9]+$")
        set(KEEPASSXC_BUILD_TYPE Release)
        set(KEEPASSXC_VERSION ${OVERRIDE_VERSION})
    else()
        set(KEEPASSXC_BUILD_TYPE Snapshot)
        set(KEEPASSXC_VERSION ${OVERRIDE_VERSION})
    endif()
else()
    if(KEEPASSXC_BUILD_TYPE STREQUAL "PreRelease")
        set(KEEPASSXC_VERSION "${KEEPASSXC_VERSION}-preview")
    elseif(KEEPASSXC_BUILD_TYPE STREQUAL "Snapshot")
        set(KEEPASSXC_VERSION "${KEEPASSXC_VERSION}-snapshot")
    endif()
endif()

if(KEEPASSXC_BUILD_TYPE STREQUAL "Release")
    set(KEEPASSXC_BUILD_TYPE_RELEASE ON)
elseif(KEEPASSXC_BUILD_TYPE STREQUAL "PreRelease")
    set(KEEPASSXC_BUILD_TYPE_PRE_RELEASE ON)
else()
    set(KEEPASSXC_BUILD_TYPE_SNAPSHOT ON)
endif()

message(STATUS "Setting up build for KeePassXC v${KEEPASSXC_VERSION}\n")

# Distribution info
set(KEEPASSXC_DIST ON)
set(KEEPASSXC_DIST_TYPE "Other" CACHE STRING "KeePassXC Distribution Type")
set_property(CACHE KEEPASSXC_DIST_TYPE PROPERTY STRINGS Snap AppImage Other)
if(KEEPASSXC_DIST_TYPE STREQUAL "Snap")
    set(KEEPASSXC_DIST_SNAP ON)
elseif(KEEPASSXC_DIST_TYPE STREQUAL "AppImage")
    set(KEEPASSXC_DIST_APPIMAGE ON)
elseif(KEEPASSXC_DIST_TYPE STREQUAL "Other")
    unset(KEEPASSXC_DIST)
endif()

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
    set(IS_32BIT TRUE)
endif()

set(CLANG_COMPILER_ID_REGEX "^(Apple)?[Cc]lang$")
if("${CMAKE_C_COMPILER}" MATCHES "clang$"
        OR "${CMAKE_EXTRA_GENERATOR_C_SYSTEM_DEFINED_MACROS}" MATCHES "__clang__"
        OR "${CMAKE_C_COMPILER_ID}" MATCHES ${CLANG_COMPILER_ID_REGEX})
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if("${CMAKE_CXX_COMPILER}" MATCHES "clang(\\+\\+)?$"
        OR "${CMAKE_EXTRA_GENERATOR_CXX_SYSTEM_DEFINED_MACROS}" MATCHES "__clang__"
        OR "${CMAKE_CXX_COMPILER_ID}" MATCHES ${CLANG_COMPILER_ID_REGEX})
    set(CMAKE_COMPILER_IS_CLANGXX 1)
endif()

macro(add_gcc_compiler_cxxflags FLAGS)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS}")
    endif()
endmacro(add_gcc_compiler_cxxflags)

macro(add_gcc_compiler_cflags FLAGS)
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS}")
    endif()
endmacro(add_gcc_compiler_cflags)

macro(add_gcc_compiler_flags FLAGS)
    add_gcc_compiler_cxxflags("${FLAGS}")
    add_gcc_compiler_cflags("${FLAGS}")
endmacro(add_gcc_compiler_flags)

# Copies of above macros that first ensure the compiler understands a given flag
# Because check_*_compiler_flag() sets -D with name, need to provide "safe" FLAGNAME
macro(check_add_gcc_compiler_cxxflag FLAG FLAGNAME)
    check_cxx_compiler_flag("${FLAG}" CXX_HAS${FLAGNAME})
    if(CXX_HAS${FLAGNAME})
        add_gcc_compiler_cxxflags("${FLAG}")
    endif()
endmacro(check_add_gcc_compiler_cxxflag)

macro(check_add_gcc_compiler_cflag FLAG FLAGNAME)
    check_c_compiler_flag("${FLAG}" CC_HAS${FLAGNAME})
    if(CC_HAS${FLAGNAME})
        add_gcc_compiler_cflags("${FLAG}")
    endif()
endmacro(check_add_gcc_compiler_cflag)

# This is the "front-end" for the above macros
# Optionally takes additional parameter(s) with language to check (currently "C" or "CXX")
macro(check_add_gcc_compiler_flag FLAG)
    string(REGEX REPLACE "[-=]" "_" FLAGNAME "${FLAG}")
    set(check_lang_spec ${ARGN})
    list(LENGTH check_lang_spec num_extra_args)
    set(langs C CXX)
    if(num_extra_args GREATER 0)
        set(langs "${check_lang_spec}")
    endif()
    if("C" IN_LIST langs)
        check_add_gcc_compiler_cflag("${FLAG}" "${FLAGNAME}")
    endif()
    if("CXX" IN_LIST langs)
        check_add_gcc_compiler_cxxflag("${FLAG}" "${FLAGNAME}")
    endif()
endmacro(check_add_gcc_compiler_flag)

add_definitions(-DQT_NO_EXCEPTIONS -DQT_STRICT_ITERATORS -DQT_NO_CAST_TO_ASCII)

if(WITH_APP_BUNDLE)
    add_definitions(-DWITH_APP_BUNDLE)
endif()

add_gcc_compiler_flags("-fno-common")
check_add_gcc_compiler_flag("-fopenmp")
add_gcc_compiler_flags("-Wall -Wextra -Wundef -Wpointer-arith -Wno-long-long")
add_gcc_compiler_flags("-Wformat=2 -Wmissing-format-attribute")
add_gcc_compiler_flags("-fvisibility=hidden")
add_gcc_compiler_cxxflags("-fvisibility-inlines-hidden")

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    check_add_gcc_compiler_flag("-Wshadow-compatible-local")
    check_add_gcc_compiler_flag("-Wshadow-local")
    add_gcc_compiler_flags("-Werror")
endif()

if (NOT HAIKU)
if((CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.8.999) OR CMAKE_COMPILER_IS_CLANGXX)
    add_gcc_compiler_flags("-fstack-protector-strong")
else()
    add_gcc_compiler_flags("-fstack-protector --param=ssp-buffer-size=4")
endif()
endif()

add_gcc_compiler_cxxflags("-Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual")
add_gcc_compiler_cflags("-Wchar-subscripts -Wwrite-strings")

if(WITH_ASAN)
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR APPLE))
        message(FATAL_ERROR "WITH_ASAN is only supported on Linux / macOS at the moment.")
    endif()

    add_gcc_compiler_flags("-fsanitize=address -DWITH_ASAN")

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(NOT (CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9))
            add_gcc_compiler_flags("-fsanitize=leak -DWITH_LSAN")
        endif()
    endif()

endif()

if(CMAKE_BUILD_TYPE_LOWER MATCHES "(release|relwithdebinfo|minsizerel)")
    add_gcc_compiler_flags("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
endif()

check_add_gcc_compiler_flag("-Werror=format-security")
check_add_gcc_compiler_flag("-Werror=implicit-function-declaration" C)
check_add_gcc_compiler_flag("-Wcast-align")

if(WITH_COVERAGE AND CMAKE_COMPILER_IS_CLANGXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
  # then:
  # $ llvm-profdata merge -sparse default.profraw -o default.profdata
  # $ llvm-cov show ./tests/${the_test_binary} \
  #      -format=html -instr-profile=default.profdata -output-dir=./coverages \
  #      `find src -iname '*.h' -or -iname '*.cpp'`
endif()

if(UNIX AND NOT APPLE)
    check_add_gcc_compiler_flag("-Qunused-arguments")
    check_add_gcc_compiler_flag("-fPIC")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-add-needed -Wl,--as-needed -Wl,--no-undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro,-z,now -pie")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-add-needed -Wl,--as-needed")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,relro,-z,now")
endif()

add_gcc_compiler_cflags("-std=c99")
add_gcc_compiler_cxxflags("-std=c++11")

check_add_gcc_compiler_flag("-fsized-deallocation" CXX)

if(APPLE AND CMAKE_COMPILER_IS_CLANGXX)
    add_gcc_compiler_cxxflags("-stdlib=libc++")
endif()

if(WITH_DEV_BUILD)
    add_definitions(-DQT_DEPRECATED_WARNINGS)
else()
    add_definitions(-DQT_NO_DEPRECATED_WARNINGS)
    add_gcc_compiler_cxxflags("-Wno-deprecated-declarations")
endif()

if(MINGW)
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
    if(NOT (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug" OR CMAKE_BUILD_TYPE_LOWER STREQUAL "relwithdebinfo"))
        # Enable DEP and ASLR
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--nxcompat -Wl,--dynamicbase")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--nxcompat -Wl,--dynamicbase")
        # Enable high entropy ASLR for 64-bit builds
        if(NOT IS_32BIT)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--high-entropy-va")
            set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--high-entropy-va")
        endif()
    endif()
endif()

if(APPLE AND WITH_APP_BUNDLE OR MINGW)
    set(PROGNAME KeePassXC)
else()
    set(PROGNAME keepassxc)
endif()

if(MINGW)
    set(CLI_INSTALL_DIR ".")
    set(PROXY_INSTALL_DIR ".")
    set(BIN_INSTALL_DIR ".")
    set(PLUGIN_INSTALL_DIR ".")
    set(DATA_INSTALL_DIR "share")
elseif(APPLE AND WITH_APP_BUNDLE)
    set(BUNDLE_INSTALL_DIR "${PROGNAME}.app/Contents")
    set(CMAKE_INSTALL_MANDIR "${BUNDLE_INSTALL_DIR}/Resources/man")
    set(CLI_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/MacOS")
    set(PROXY_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/MacOS")
    set(BIN_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/MacOS")
    set(PLUGIN_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/PlugIns")
    set(DATA_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/Resources")
else()
    include(GNUInstallDirs)

    set(CLI_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
    set(PROXY_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
    set(PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/keepassxc")
    set(DATA_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/keepassxc")
endif()

if(WITH_TESTS)
    enable_testing()
endif(WITH_TESTS)

if(WITH_COVERAGE)
    # Include code coverage, use with -DCMAKE_BUILD_TYPE=Debug
    include(CodeCoverage)
    set(COVERAGE_GCOVR_EXCLUDES
            "\\(.+/\\)?tests/.\\*"
            ".\\*/moc_\\[^/\\]+\\.cpp"
            ".\\*/ui_\\[^/\\]+\\.h"
            "\\(.+/\\)?zxcvbn/.\\*")
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_html(
            NAME coverage
            EXECUTABLE $(MAKE) && $(MAKE) test
    )
endif()

include(CLangFormat)

set(QT_COMPONENTS Core Network Concurrent Gui Svg Widgets Test LinguistTools)
if(UNIX AND NOT APPLE)
    find_package(Qt5 COMPONENTS ${QT_COMPONENTS} DBus X11Extras REQUIRED)
elseif(APPLE)
    find_package(Qt5 COMPONENTS ${QT_COMPONENTS} REQUIRED HINTS
            /usr/local/opt/qt/lib/cmake
            /usr/local/Cellar/qt/*/lib/cmake
            /opt/homebrew/opt/qt/lib/cmake
            ENV PATH)
    find_package(Qt5 COMPONENTS MacExtras HINTS
            /usr/local/opt/qt/lib/cmake
            /usr/local/Cellar/qt/*/lib/cmake
            /opt/homebrew/opt/qt/lib/cmake
            ENV PATH)
else()
    find_package(Qt5 COMPONENTS ${QT_COMPONENTS} REQUIRED)
endif()

if(Qt5Core_VERSION VERSION_LESS "5.2.0")
    message(FATAL_ERROR "Qt version 5.2.0 or higher is required")
endif()

get_filename_component(Qt5_PREFIX ${Qt5_DIR}/../../.. REALPATH)

# Process moc automatically
set(CMAKE_AUTOMOC ON)
# Process .ui files automatically
set(CMAKE_AUTOUIC ON)
# Process .qrc files automatically
set(CMAKE_AUTORCC ON)

if(APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
    find_program(MACDEPLOYQT_EXE macdeployqt HINTS ${Qt5_PREFIX}/bin ENV PATH)
    if(NOT MACDEPLOYQT_EXE)
        message(FATAL_ERROR "macdeployqt is required to build on macOS")
    endif()
    message(STATUS "Using macdeployqt: ${MACDEPLOYQT_EXE}")
elseif(MINGW)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS ${Qt5_PREFIX}/bin ENV PATH)
    if(NOT WINDEPLOYQT_EXE)
        message(FATAL_ERROR "windeployqt is required to build on Windows")
    endif()
    message(STATUS "Using windeployqt: ${WINDEPLOYQT_EXE}")
endif()

# Debian sets the the build type to None for package builds.
# Make sure we don't enable asserts there.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_NONE QT_NO_DEBUG)

# Find Botan2
find_package(Botan2 REQUIRED)
if(BOTAN2_VERSION VERSION_LESS "2.11.0")
    message(FATAL_ERROR "Botan2 2.11.0 or higher is required")
endif()
include_directories(SYSTEM ${BOTAN2_INCLUDE_DIR})

# Find zlib
find_package(ZLIB REQUIRED)
if(ZLIB_VERSION_STRING VERSION_LESS "1.2.0")
    message(FATAL_ERROR "zlib 1.2.0 or higher is required to use the gzip format")
endif()
include_directories(SYSTEM ${ZLIB_INCLUDE_DIR})

# QREncode required for TOTP
find_package(QREncode REQUIRED)

if(UNIX)
    check_cxx_source_compiles("#include <sys/prctl.h>
    int main() { prctl(PR_SET_DUMPABLE, 0); return 0; }"
            HAVE_PR_SET_DUMPABLE)

    check_cxx_source_compiles("#include <malloc.h>
    int main() { return 0; }"
            HAVE_MALLOC_H)

    check_cxx_source_compiles("#include <malloc.h>
    int main() { malloc_usable_size(NULL, 0); return 0; }"
            HAVE_MALLOC_USABLE_SIZE)

    check_cxx_source_compiles("#include <sys/resource.h>
    int main() {
      struct rlimit limit;
      limit.rlim_cur = 0;
      limit.rlim_max = 0;
      setrlimit(RLIMIT_CORE, &limit);
      return 0;
    }" HAVE_RLIMIT_CORE)

    if(APPLE)
        check_cxx_source_compiles("#include <sys/types.h>
      #include <sys/ptrace.h>
      int main() { ptrace(PT_DENY_ATTACH, 0, 0, 0); return 0; }"
                HAVE_PT_DENY_ATTACH)
    endif()
endif()

include_directories(SYSTEM ${ZLIB_INCLUDE_DIR})

add_subdirectory(src)
add_subdirectory(share)
if(WITH_TESTS)
    add_subdirectory(tests)
endif(WITH_TESTS)

if(WITH_XC_DOCS)
    add_subdirectory(docs)
endif()

if(PRINT_SUMMARY)
    # This will print ENABLED, REQUIRED and DISABLED
    feature_summary(WHAT ALL)
else()
    # This will only print ENABLED and DISABLED feature
    feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
    feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
endif()
