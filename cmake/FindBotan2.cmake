# Copyright (c) 2018 Ribose Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

#.rst:
# FindBotan2
# -----------
#
# Find the botan-2 library.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` targets:
#
# ``Botan2::Botan2``
#   The botan-2 library, if found.
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   BOTAN2_FOUND          - true if the headers and library were found
#   BOTAN2_INCLUDE_DIRS   - where to find headers
#   BOTAN2_LIBRARIES      - list of libraries to link
#   BOTAN2_VERSION        - library version that was found, if any

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig QUIET)
pkg_check_modules(PC_BOTAN2 QUIET botan-2)

# find the headers
find_path(BOTAN2_INCLUDE_DIR
  NAMES botan/version.h
  HINTS
    ${PC_BOTAN2_INCLUDEDIR}
    ${PC_BOTAN2_INCLUDE_DIRS}
  PATH_SUFFIXES botan-2
)

# find the library
find_library(BOTAN2_LIBRARY
  NAMES botan-2 libbotan-2 botan
  HINTS
    ${PC_BOTAN2_LIBDIR}
    ${PC_BOTAN2_LIBRARY_DIRS}
)

# determine the version
if(PC_BOTAN2_VERSION)
    set(BOTAN2_VERSION ${PC_BOTAN2_VERSION})
elseif(BOTAN2_INCLUDE_DIR AND EXISTS "${BOTAN2_INCLUDE_DIR}/botan/build.h")
    file(STRINGS "${BOTAN2_INCLUDE_DIR}/botan/build.h" botan2_version_str
      REGEX "^#define[\t ]+(BOTAN_VERSION_[A-Z]+)[\t ]+[0-9]+")

    string(REGEX REPLACE ".*#define[\t ]+BOTAN_VERSION_MAJOR[\t ]+([0-9]+).*"
           "\\1" _botan2_version_major "${botan2_version_str}")
    string(REGEX REPLACE ".*#define[\t ]+BOTAN_VERSION_MINOR[\t ]+([0-9]+).*"
           "\\1" _botan2_version_minor "${botan2_version_str}")
    string(REGEX REPLACE ".*#define[\t ]+BOTAN_VERSION_PATCH[\t ]+([0-9]+).*"
           "\\1" _botan2_version_patch "${botan2_version_str}")
    set(BOTAN2_VERSION "${_botan2_version_major}.${_botan2_version_minor}.${_botan2_version_patch}"
                       CACHE INTERNAL "The version of Botan which was detected")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Botan2
  REQUIRED_VARS BOTAN2_LIBRARY BOTAN2_INCLUDE_DIR
  VERSION_VAR BOTAN2_VERSION
)

if (BOTAN2_FOUND)
  set(BOTAN2_INCLUDE_DIRS ${BOTAN2_INCLUDE_DIR} ${PC_BOTAN2_INCLUDE_DIRS})
  set(BOTAN2_LIBRARIES ${BOTAN2_LIBRARY})
endif()

if (BOTAN2_FOUND AND NOT TARGET Botan2::Botan2)
  # create the new library target
  add_library(Botan2::Botan2 UNKNOWN IMPORTED)
  # set the required include dirs for the target
  if (BOTAN2_INCLUDE_DIRS)
    set_target_properties(Botan2::Botan2
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${BOTAN2_INCLUDE_DIRS}"
    )
  endif()
  # set the required libraries for the target
  if (EXISTS "${BOTAN2_LIBRARY}")
    set_target_properties(Botan2::Botan2
      PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${BOTAN2_LIBRARY}"
    )
  endif()
endif()

mark_as_advanced(BOTAN2_INCLUDE_DIR BOTAN2_LIBRARY)
