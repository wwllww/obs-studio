#include <obs-module.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-webrtc", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS WebRTC-Core";
}

extern "C" struct obs_output_info webrtc_core_info;
extern "C" struct obs_service_info webrtc_custom_service;

bool obs_module_load(void)
{
#ifdef _WIN32
	WSADATA wsad;
	WSAStartup(MAKEWORD(2, 2), &wsad);
#endif

	obs_register_output(&webrtc_core_info);
	obs_register_service(&webrtc_custom_service);
	return true;
}

void obs_module_unload(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}
