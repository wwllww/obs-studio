#include "webrtc-core.h"
#include "peer-connection.h"

extern "C" void set_request_key_frame(struct encoder_packet *packet);
extern "C" void set_repeat_header(struct obs_output *output);

void LogCallback(int level, const char *file, int line, const char *function,
		 const char *fmt, va_list vl)
{
	std::string slog;
	slog.resize(4096, '\0');
	size_t len = vsnprintf(const_cast<char *>(slog.data()), slog.size(),
			       fmt, vl);
	slog.resize(len, '\0');

	std::string strlog = std::string(file) + " " + std::string(function) +
			     ":" + std::to_string(line) + " " + slog;

	switch (level) {
	case webrtccore::kError:
		log_error(strlog.c_str());
		break;
	case webrtccore::kWarning:
		log_warn(strlog.c_str());
		break;
	case webrtccore::kNotice:
	case webrtccore::kInfo:
		log_info(strlog.c_str());
		break;
	case webrtccore::kDebug:
		log_debug(strlog.c_str());
		break;
	default:
		log_info(strlog.c_str());
		break;
	}
}
#define MILLISECOND_DEN 1000

static int32_t get_ms_time(struct encoder_packet *packet, int64_t val)
{
	return (int32_t)(val * MILLISECOND_DEN / packet->timebase_den);
}

static const char *webrtc_stream_getname(void *unused)
{
	return obs_module_text("WebRTCStream");
}

static void webrtc_stream_destroy(void *data)
{
	peer_connection *webrtc_peer =
		reinterpret_cast<peer_connection *>(data);
	webrtc_peer->stopUdpDownloadThread();
	delete webrtc_peer;
}

static void *webrtc_stream_create(obs_data_t *settings, obs_output_t *output)
{
	webrtccore::LogSetCallback(LogCallback);
	/*creat peer*/
	peer_connection *webrtc_peer = new peer_connection();
	webrtc_peer->SetObsOutput(output);
	return webrtc_peer;
}

static void webrtc_stream_stop(void *data, uint64_t ts)
{
	peer_connection *webrtc_peer =
		reinterpret_cast<peer_connection *>(data);
	if (webrtc_peer) {
		obs_output_end_data_capture(webrtc_peer->GetObsOutput());
		webrtc_peer->reset();
	}
}

static bool webrtc_stream_start(void *data)
{
	peer_connection *webrtc_peer =
		reinterpret_cast<peer_connection *>(data);
	webrtc_peer->reset();

	obs_output_t *output = webrtc_peer->GetObsOutput();
	if (!obs_output_can_begin_data_capture(output, 0))
		return false;
	set_repeat_header(output);
	if (!obs_output_initialize_encoders(output, 0))
		return false;
	obs_service_t *service = obs_output_get_service(output);
	if (!service) {
		obs_output_set_last_error(
			output,
			"An unexpected error occurred during stream startup.");
		obs_output_signal_stop(output, OBS_OUTPUT_BAD_PATH);
		webrtc_peer->reset();
		return false;
	}

	// Extract setting from service
	std::string url = obs_service_get_url(service)
				  ? obs_service_get_url(service)
				  : "";
	if (url.empty()) {
		obs_output_set_last_error(output, "Url is empty.");
		obs_output_signal_stop(output, OBS_OUTPUT_INVALID_STREAM);
		webrtc_peer->reset();
		return false;
	}

	webrtccore::PeerConnectionFactoryInterface *peer_connection_factory =
		new webrtccore::PeerConnectionFactoryInterface();

	webrtccore::PeerInitializeParamInterface *pc_init_param =
		peer_connection_factory->CreatePeerInitializeParam();

	pc_init_param->SetLogLevel(webrtccore::kNotice);
	pc_init_param->SetIceTimeOutMs(10000);
	pc_init_param->SetVideoMaxNackQueLen(8192);
	pc_init_param->SetAudioMaxNackQueLen(2048);
	pc_init_param->SetMaxNackWaitTime(2000);
	pc_init_param->SetMaxNackTimes(10);

	pc_init_param->AddAudioExtMap(
		1, "uurn:ietf:params:rtp-hdrext:ssrc-audio-level");
	pc_init_param->AddVideoExtMap(4, "urn:3gpp:video-orientation");

	std::vector<std::string> audio_feedback_types;
	std::map<std::string, std::string> audio_format_parameters;
	audio_feedback_types.push_back("nack");
	pc_init_param->AddAudioRtpMap("opus", 111, 2, 48000,
				      audio_feedback_types,
				      audio_format_parameters);

	std::vector<std::string> video_feedback_types;
	std::map<std::string, std::string> video_format_parameters;
	video_feedback_types.push_back("nack");
	video_feedback_types.push_back("nack pli");
	video_feedback_types.push_back("ccm fir");
	video_feedback_types.push_back("goog-remb");

	video_format_parameters["packetization-mode"] = "1";
	video_format_parameters["level-asymmetry-allowed"] = "1";

	video_format_parameters["profile-level-id"] = "42001f";
	pc_init_param->AddVideoRtpMap("H264", 102, 2, 90000,
				      video_feedback_types,
				      video_format_parameters);

	video_format_parameters["profile-level-id"] = "42e01f";
	pc_init_param->AddVideoRtpMap("H264", 125, 2, 90000,
				      video_feedback_types,
				      video_format_parameters);

	webrtccore::PeerConnectionInterface *pc =
		peer_connection_factory->CreatePeerConnection(
			webrtc_peer, 0, "obs_stream", pc_init_param);
	webrtc_peer->SetPC(pc);
	pc->AddLocalVideoTrack(rand(), "OBSVideo");
	pc->AddLocalAudioTrack(rand(), "OBSAudio");

	log_info("Publish API URL: %s", url.c_str());
	webrtc_peer->SdpExchange("obs_stream", url);

	{
		delete pc_init_param;
		pc_init_param = NULL;
		delete peer_connection_factory;
		peer_connection_factory = nullptr;
	}
	return true;
}

static void webrtc_stream_encoded_data(void *data,
				       struct encoder_packet *packet)
{
	peer_connection *webrtc_peer =
		reinterpret_cast<peer_connection *>(data);

	MutexLock lock(webrtc_peer->GetMute());
	if (!webrtc_peer->GetPC()) {
		return;
	}
	/* encoder fail */
	if (!packet) {
		log_error("webrtc_stream_encoded_data encoder failed");
		return;
	}

	uint32_t ssrc = 0;
	uint32_t payload_type = 0;
	webrtccore::MediaType media_type = webrtccore::kMediaVideo;
	if (packet->type == OBS_ENCODER_VIDEO) {
		ssrc = webrtc_peer->GetLocalVideoSsrc();
		payload_type = webrtc_peer->GetLocalVideoPayloadType();
		bool &request_key_frame = webrtc_peer->IsRequestKeyFrame();
		if (request_key_frame) {
			set_request_key_frame(packet);
			request_key_frame = false;
		}
	} else {
		ssrc = webrtc_peer->GetLocalAudioSsrc();
		payload_type = webrtc_peer->GetLocalAudioPayloadType();
		media_type = webrtccore::kMediaAudio;
	}

	std::unique_ptr<MediaFrame> raw_data(
		new MediaFrame(media_type, (char *)packet->data, packet->size,
			       get_ms_time(packet, packet->dts), ssrc,
			       payload_type, webrtccore::kVideoRotation0, 960));

	webrtc_peer->AddSendBytes(packet->type, packet->size);
	webrtc_peer->GetPC()->FeedMediaData(std::move(raw_data));
}

static void webrtc_stream_defaults(obs_data_t *defaults) {}

static obs_properties_t *webrtc_stream_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();
	return props;
}

static uint64_t webrtc_stream_total_bytes_sent(void *data)
{
	peer_connection *webrtc_peer =
		reinterpret_cast<peer_connection *>(data);
	return webrtc_peer->GetTotalBytesSent();
}

static float webrtc_stream_congestion(void *data)
{
	return 0;
}

extern "C" {
#ifdef _WIN32
struct obs_output_info webrtc_core_info = {
	"webrtc_core_output",                                    //id
	OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_SERVICE, //flags
	webrtc_stream_getname,                                   //get_name
	webrtc_stream_create,                                    //create
	webrtc_stream_destroy,                                   //destroy
	webrtc_stream_start,                                     //start
	webrtc_stream_stop,                                      //stop
	nullptr,                                                 //raw_video
	nullptr,                                                 //raw_audio
	webrtc_stream_encoded_data,     //encoded_packet
	nullptr,                        //update
	webrtc_stream_defaults,         //get_defaults
	webrtc_stream_properties,       //get_properties
	nullptr,                        //unused1 (formerly pause)
	webrtc_stream_total_bytes_sent, //get_total_bytes
	nullptr,                        //get_dropped_frames
	nullptr,                        //type_data
	nullptr,                        //free_type_data
	webrtc_stream_congestion,       //get_congestion
	nullptr,                        //get_connect_time_ms
	"h264",                         //encoded_video_codecs
	"opus",                         //encoded_audio_codecs
	nullptr                         //raw_audio2
};

#else
struct obs_output_info webrtc_core_info = {
	.id = "webrtc_core_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_SERVICE,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "opus",
	.get_name = webrtc_stream_getname,
	.create = webrtc_stream_create,
	.destroy = webrtc_stream_destroy,
	.start = webrtc_stream_start,
	.stop = webrtc_stream_stop,
	.encoded_packet = webrtc_stream_encoded_data,
	.get_defaults = webrtc_stream_defaults,
	.get_properties = webrtc_stream_properties,
	.get_total_bytes = webrtc_stream_total_bytes_sent,
	.get_congestion = webrtc_stream_congestion,
};
#endif
}
