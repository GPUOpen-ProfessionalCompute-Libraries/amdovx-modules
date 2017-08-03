#.rst:
# FindOpenVX
# --------
#
# Try to find OpenVX
#
# Once done this will define::
#
#   OpenVX_FOUND          - True if OpenVX was found
#   OpenVX_INCLUDE_DIRS   - include directories for OpenVX
#   OpenVX_LIBRARIES      - link against this library to use OpenVX
#   OpenVX_VERSION_STRING - Highest supported OpenVX version (eg. 1.0)
#   OpenVX_VERSION_MAJOR  - The major version of the OpenVX implementation
#   OpenVX_VERSION_MINOR  - The minor version of the OpenVX implementation
#
# The module will also define two cache variables::
#
#   OpenVX_INCLUDE_DIR    - the OpenVX include directory
#   OpenVX_LIBRARY        - the path to the OpenVX library
#
#
# also defined, but not for general use are
#
# ::
#
#   OPENVX_LIBRARY, where to find the OpenVX library.

#=============================================================================
# Copyright 2016 sheldon robinson
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

find_path(OpenVX_INCLUDE_DIR NAMES VX/vx.h)
mark_as_advanced(OpenVX_INCLUDE_DIR)

set(OPENVX_NAMES NAMES 
	openvx 
	libopenvx
	# NVIDIA VisionWorks
	visionworks
	libvisionworks 
	)
find_library(OpenVX_LIBRARY NAMES ${OPENVX_NAMES} )
mark_as_advanced(OpenVX_LIBRARY)

if( EXISTS "${OpenVX_INCLUDE_DIR}/VX/vx.h")
    file(STRINGS "${OpenVX_INCLUDE_DIR}/VX/vx.h" openvx_underscored_version_str
         REGEX "^#define[\t ]+VX_VERSION[\t ]*VX_VERSION_([0-9])+_([0-9])+")

    string(REGEX REPLACE "^#define[\t ]+VX_VERSION[\t ]*VX_VERSION_(([0-9])+_([0-9])+)*$" "\\1"
           VERSION_STRING "${openvx_underscored_version_str}")
    if(VERSION_STRING)
	string(REPLACE "_" "." VERSION_STRING "${VERSION_STRING}")
	set(OpenVX_VERSION_STRING ${VERSION_STRING} PARENT_SCOPE)
	string(REGEX MATCHALL "[0-9]+" version_components "${VERSION_STRING}")
      	list(GET version_components 0 major_version)
      	list(GET version_components 1 minor_version)
      	set(OpenVX_VERSION_MAJOR ${major_version} PARENT_SCOPE)
      	set(OpenVX_VERSION_MINOR ${minor_version} PARENT_SCOPE)
    endif()
    unset(VERSION_STRING)
    unset(openvx_underscored_version_str)
endif()

# handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  OpenVX
  FOUND_VAR OpenVX_FOUND
  REQUIRED_VARS OpenVX_LIBRARY OpenVX_INCLUDE_DIR
  VERSION_VAR OpenVX_VERSION_STRING)

if(OpenVX_FOUND)
  set(OpenVX_LIBRARIES ${OpenVX_LIBRARY})
  set(OpenVX_INCLUDE_DIRS ${OpenVX_INCLUDE_DIR})
endif()

