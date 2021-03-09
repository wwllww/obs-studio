project(obs-webrtc)
#find_package(webrtc-core REQUIRED)

set(obs-webrtc_PLATFORM_DEPS
		webrtc-core)
include_directories(${DepsPath}/include ${CMAKE_SOURCE_DIR}/deps/libcaption)
if(MSVC)
        link_directories(${DepsPath}/bin)
	set(obs-webrtc_PLATFORM_DEPS
		${obs-webrtc_PLATFORM_DEPS}
		w32-pthreads
		ws2_32
		Iphlpapi
		srtp2
		libeay32
		ssleay32
                libcurl)
	add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
	add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
	add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
else()
        link_directories(${DepsPath}/lib)
	set(obs-webrtc_PLATFORM_DEPS
		${obs-webrtc_PLATFORM_DEPS}
                curl
		srtp2
		crypto
		ssl)
endif()

set(obs-webrtc_HEADERS
	common-net-utils.h
	peer-connection.h
	rtc-net-utils.h
	rtc-socket-interface.h
	rtc-udp-socket.h
	webrtc-core.h)

set(obs-webrtc_SOURCES
	common-net-utils.cpp
	obs-webrtc.cpp
	peer-connection.cpp
	rtc-net-utils.cpp
	rtc-udp-proxy.cpp
	rtc-udp-socket.cpp
	webrtc-core.cpp
	webrtc-custom.c)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS webrtc module")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obs-webrtc.rc)
	list(APPEND obs-webrtc_SOURCES
		obs-webrtc.rc)
endif()

add_library(obs-webrtc MODULE
	${obs-webrtc_HEADERS}
	${obs-webrtc_SOURCES})
target_link_libraries(obs-webrtc
	libobs
	${obs-webrtc_PLATFORM_DEPS})
set_target_properties(obs-webrtc PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(obs-webrtc data)
