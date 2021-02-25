# Once done these will be defined:
#
#  LIBWEBRTC_CORE_FOUND
#  LIBWEBRTC_CORE_INCLUDE_DIRS
#  LIBWEBRTC_CORE_LIBRARIES
#
# For use in OBS: 
#
#  LIBWEBRTC_CORE_INCLUDE_DIR

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_WEBRTC_CORE QUIET webrtc-core)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

find_path(LIBWEBRTC_CORE_INCLUDE_DIR
	NAMES webrtc-core/webrtc_core_interface.h
	HINTS
		ENV webrtc-corePath${_lib_suffix}
		ENV webrtc-corePath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${webrtc-corePath${_lib_suffix}}
		${webrtc-corePath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_WEBRTC_CORE_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	PATH_SUFFIXES
		include)

find_library(WEBRTC_CORE_LIB
	NAMES ${_WEBRTC_CORE_LIBRARIES} webrtc-core
	HINTS
		ENV webrtc-corePath${_lib_suffix}
		ENV webrtc-corePath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${webrtc-corePath${_lib_suffix}}
		${webrtc-corePath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_WEBRTC_CORE_LIBRARY_DIRS}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib /sw/lib
	PATH_SUFFIXES
		lib${_lib_suffix} lib
		libs${_lib_suffix} libs
		bin${_lib_suffix} bin
		../lib${_lib_suffix} ../lib
		../libs${_lib_suffix} ../libs
		../bin${_lib_suffix} ../bin)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(webrtc-core DEFAULT_MSG WEBRTC_CORE_LIB LIBWEBRTC_CORE_INCLUDE_DIR)
mark_as_advanced(LIBWEBRTC_CORE_INCLUDE_DIR WEBRTC_CORE_LIB)

if(LIBWEBRTC_CORE_FOUND)
	set(LIBWEBRTC_CORE_INCLUDE_DIR ${LIBWEBRTC_CORE_INCLUDE_DIR})
	set(WEBRTC_CORE_LIB ${WEBRTC_CORE_LIB})
endif()
