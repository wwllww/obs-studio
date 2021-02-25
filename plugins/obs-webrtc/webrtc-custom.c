/* Copyright Dr. Alex. Gouaillard (2015, 2020) */

#include <obs-module.h>
#include <obs-internal.h>
struct webrtc_custom {
	char *server;
	char *key;
	char *output;
};

void set_request_key_frame(struct encoder_packet *packet)
{
	packet->encoder->info.set_request_key_frame(
		packet->encoder->context.data, true);
}

static const char *webrtc_custom_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "WebRTC Custom";
}

static void webrtc_custom_update(void *data, obs_data_t *settings)
{
	struct webrtc_custom *service = data;

	bfree(service->server);
	bfree(service->key);
	bfree(service->output);

	service->server = bstrdup(obs_data_get_string(settings, "server"));
	service->key = bstrdup(obs_data_get_string(settings, "key"));
	service->output = bstrdup("webrtc_core_output");
}

static void webrtc_custom_destroy(void *data)
{
	struct webrtc_custom *service = data;

	bfree(service->server);
	bfree(service->key);
	bfree(service->output);
	bfree(service);
}

static void *webrtc_custom_create(obs_data_t *settings, obs_service_t *service)
{
	struct webrtc_custom *data = bzalloc(sizeof(struct webrtc_custom));
	webrtc_custom_update(data, settings);

	UNUSED_PARAMETER(service);
	return data;
}

static bool use_auth_modified(obs_properties_t *ppts, obs_property_t *p,
			      obs_data_t *settings)
{
	p = obs_properties_get(ppts, "server");
	obs_property_set_visible(p, true);

	p = obs_properties_get(ppts, "key");
	obs_property_set_visible(p, false);
	return true;
}

static obs_properties_t *webrtc_custom_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_get(ppts, "server");
	obs_property_set_visible(p, true);

	p = obs_properties_get(ppts, "key");
	obs_property_set_visible(p, false);
	return ppts;
}

static const char *webrtc_custom_url(void *data)
{
	struct webrtc_custom *service = data;
	return service->server;
}

static const char *webrtc_custom_key(void *data)
{
	struct webrtc_custom *service = data;
	return service->key;
}

static const char *webrtc_custom_username(void *data)
{
	UNUSED_PARAMETER(data);
	return "";
}

static const char *webrtc_custom_password(void *data)
{
	UNUSED_PARAMETER(data);
	return "";
}

static const char *webrtc_custom_get_output_type(void *data)
{
	struct webrtc_custom *service = data;
	return service->output;
}

struct obs_service_info webrtc_custom_service = {
	.id = "webrtc_custom",
	.get_name = webrtc_custom_name,
	.create = webrtc_custom_create,
	.destroy = webrtc_custom_destroy,
	.update = webrtc_custom_update,
	.get_properties = webrtc_custom_properties,
	.get_url = webrtc_custom_url,
	.get_key = webrtc_custom_key,
	.get_username = webrtc_custom_username,
	.get_password = webrtc_custom_password,
	.get_output_type = webrtc_custom_get_output_type};
