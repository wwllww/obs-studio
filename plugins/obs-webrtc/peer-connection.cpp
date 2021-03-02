#include "peer-connection.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WS2tcpip.h>
#pragma warning(disable:4100)
#else
#include <arpa/inet.h>
#include <sys/time.h>
#pragma  GCC diagnostic ignored  "-Wunused"
#pragma  GCC diagnostic ignored  "-Wunused-parameter"
#endif

#ifdef _WIN32
static const unsigned __int64 epoch = ((unsigned __int64)116444736000000000ULL);
int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
	FILETIME file_time;
	SYSTEMTIME system_time;
	ULARGE_INTEGER ularge;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	ularge.LowPart = file_time.dwLowDateTime;
	ularge.HighPart = file_time.dwHighDateTime;

	tp->tv_sec = (long)((ularge.QuadPart - epoch) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}
#endif

uint64_t GetTimestampMs()
{
	timeval tm;
	gettimeofday(&tm, NULL);
	uint64_t TimeMs = tm.tv_sec * 1000 + tm.tv_usec / 1000;
	return TimeMs;
}


peer_connection::peer_connection()
{
	is_ice_break_ = false;
	is_peer_break_ = false;
	pthread_mutex_init(&mutex_, nullptr);
}

peer_connection::~peer_connection()
{
	if (peer_connection_) {
		delete peer_connection_;
		peer_connection_ = NULL;
	}
	if (m_pUdpProxy) {
		delete m_pUdpProxy;
		m_pUdpProxy = nullptr;
	}
	pthread_mutex_destroy(&mutex_);
}

void peer_connection::OnIceConnectionChange(
	webrtccore::IceConnectionState new_state)
{
	if (new_state == webrtccore::kIceConnectionFailed ||
	    new_state == webrtccore::kIceConnectionClosed) {
		is_ice_break_ = true;
	}
	log_info("OnIceConnectionChange IceConnectionState:%d", new_state);
}

void peer_connection::OnConnectionChange(
	webrtccore::PeerConnectionState new_state)
{
	if (new_state == webrtccore::kDisconnected ||
	    new_state == webrtccore::kFailed ||
	    new_state == webrtccore::kClosed) {
		is_peer_break_ = true;
	}
	log_info("OnConnectionChange PeerConnectionState:%d", new_state);
	if (new_state == webrtccore::kConnected) {
		log_info("Begin data capture...");
		obs_output_begin_data_capture(output_, 0);
	}
}

void peer_connection::OnAddAudioTrack(
	uint32_t ssrc, const std::string &label,
	const std::map<uint32_t, webrtccore::AudioCodeInfo> &codec_info_map)
{
}

void peer_connection::OnRemoveAudioTrack(uint32_t ssrc) {}

void peer_connection::OnAddVideoTrack(
	uint32_t ssrc, const std::string &label,
	const std::map<uint32_t, webrtccore::VideoCodeInfo> &codec_info_map)
{
}

void peer_connection::OnRemoveVideoTrack(uint32_t ssrc) {}

void peer_connection::OnRecvMeidaData(
	std::unique_ptr<webrtccore::MeidaData> media_data)
{
	log_debug("%s, OnRecvMeidaData media_type_: %d ,len: %d\r\n",
		  __FUNCTION__, media_data->media_type_, media_data->len_);
	if (webrtccore::kMediaVideo == media_data->media_type_) {
	}
}

void peer_connection::OnRecvChannelData(std::unique_ptr<webrtccore::Packet> data)
{
}

void peer_connection::OnSendDataToRemote(
	std::unique_ptr<webrtccore::Packet> data,
	const webrtccore::NetAddr &addr)
{
	UdpSocketSend(data->data_, data->len_, addr);
}

void peer_connection::OnRequestIFrame(uint32_t ssrc)
{
	log_info("OnRequestIFrame ssrc %u", ssrc);
	if (last_request_key_frame_time_ == 0 ||
	    GetTimestampMs() - last_request_key_frame_time_ > 500) {
		request_key_frame_ = true;
		last_request_key_frame_time_ = GetTimestampMs();
	}
}

void peer_connection::OnSendDumpData(char *dump_data, int32_t len) {}

void peer_connection::OnAddLocalAudioTrack(
	uint32_t ssrc, const std::string &label,
	const std::map<uint32_t, webrtccore::AudioCodeInfo> &codec_info_map)
{
	log_info("add Audio Track ssrc:%u label:%s\r\n", ssrc, label.c_str());

	auto itor = codec_info_map.begin();

	while (itor != codec_info_map.end()) {
		const webrtccore::AudioCodeInfo *audio_codec_info =
			&(itor->second);
		log_info(
			" payload_type:%d codec_type:%u sample_rate:%d channels:%d\r\n",
			audio_codec_info->payload_type,
			audio_codec_info->codec_type,
			audio_codec_info->sample_rate,
			audio_codec_info->channels);

		if ((0 == local_audio_payloadtype_) &&
		    (webrtccore::kAudioCodecOpus ==
		     audio_codec_info->codec_type)) {
			local_audio_payloadtype_ =
				audio_codec_info->payload_type;
		}

		itor++;
	}

	local_audio_codec_info_map_ = codec_info_map;
	local_audio_ssrc_ = ssrc;
}

void peer_connection::OnAddLocalVideoTrack(
	uint32_t ssrc, const std::string &label,
	const std::map<uint32_t, webrtccore::VideoCodeInfo> &codec_info_map)
{
	log_info("local add Video Track ssrc:%u\r\n", ssrc);
	auto itor = codec_info_map.begin();

	while (itor != codec_info_map.end()) {
		const webrtccore::VideoCodeInfo *video_codec_info =
			&(itor->second);
		log_info(
			"local payload_type:%d codec_type:%u profile_level_id:%s\r\n",
			video_codec_info->payload_type,
			video_codec_info->codec_type,
			video_codec_info->profile_level_id.c_str());

		if ((0 == local_video_payloadtype_) &&
		    (webrtccore::kVideoCodeH264 ==
		     video_codec_info->codec_type)) {
			local_video_payloadtype_ =
				video_codec_info->payload_type;
		}

		itor++;
	}

	local_video_codec_info_map_ = codec_info_map;
	local_video_ssrc_ = ssrc;
}

void peer_connection::UdpSocketSend(char *data, int32_t len,
				    const webrtccore::NetAddr &addr)
{
	struct sockaddr_in remote_addr;
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = addr.port;
	inet_pton(AF_INET, addr.ip.c_str(), (void *)&(remote_addr.sin_addr));

	if (m_pUdpProxy) {
		m_pUdpProxy->SendTo(remote_addr.sin_addr.s_addr,
				    ntohs(remote_addr.sin_port), data, len);
	}
}

size_t write_callback(void *data, size_t size, size_t nmemb, void *userdata)
{
	Response *r;
	r = reinterpret_cast<Response *>(userdata);
	r->body.append(reinterpret_cast<char *>(data), size * nmemb);
	return (size * nmemb);
}

CURLcode peer_connection::HttpPost(const std::string &url, std::string str_json,
				   Response &res, int time_out)
{
	CURLcode ret;
	CURL *pcurl = curl_easy_init();
	struct curl_slist *headers = NULL;
	if (pcurl == NULL) {
		return CURLE_FAILED_INIT;
	}

	ret = curl_easy_setopt(pcurl, CURLOPT_URL, url.c_str());
	ret = curl_easy_setopt(pcurl, CURLOPT_POST, 1L);
	headers = curl_slist_append(headers, "content-type:application/sdp");
	ret = curl_easy_setopt(pcurl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(pcurl, CURLOPT_POSTFIELDS, str_json.c_str());
	curl_easy_setopt(pcurl, CURLOPT_POSTFIELDSIZE, str_json.size());
	ret = curl_easy_setopt(pcurl, CURLOPT_TIMEOUT, time_out);
	ret = curl_easy_setopt(pcurl, CURLOPT_WRITEFUNCTION, write_callback);
	ret = curl_easy_setopt(pcurl, CURLOPT_WRITEDATA, (void *)&res);
	// fix Andorid request return code == 60
	curl_easy_setopt(pcurl, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(pcurl, CURLOPT_SSL_VERIFYHOST, false);
	curl_easy_setopt(pcurl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pcurl, CURLOPT_MAXREDIRS, static_cast<int64_t>(3));
	log_info("HttpPost req url:%s,time_out %d", url.c_str(), time_out);
	ret = curl_easy_perform(pcurl);

	if (ret != CURLE_OK) {
		switch (ret) {
		case CURLE_OPERATION_TIMEDOUT:
			res.code = ret;
			res.body = "Operation Timeout.";
			break;
		case CURLE_SSL_CERTPROBLEM:
			res.code = ret;
			res.body = curl_easy_strerror(ret);
			break;
		default:
			res.body = "Failed to query.";
			res.code = -1;
		}
	} else {
		int64_t http_code = 0;
		curl_easy_getinfo(pcurl, CURLINFO_RESPONSE_CODE, &http_code);
		res.code = static_cast<int>(http_code);
	}

	log_info("HttpPost res code:%d", res.code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(pcurl);
	return ret;
}

webrtccore::PeerConnectionInterface *peer_connection::GetPC()
{
	return peer_connection_;
}

void peer_connection::SetPC(webrtccore::PeerConnectionInterface *pc)
{
	peer_connection_ = pc;
}

void peer_connection::OnTime()
{
	uint64_t time_ms = GetTimestampMs();
	if (peer_connection_) {
		peer_connection_->OnTime(time_ms);
	}
}

void peer_connection::SdpExchange(const std::string &stream_id,
				  const std::string &live_url)
{
	webrtccore::RTCOfferAnswerOptions options(0, 0, webrtccore::kAddrIpv4,
						  "127.0.0.1", 8000, false,
						  webrtccore::kActpass);
	std::string offer_sdp;
	/*get sdp*/
	peer_connection_->CreateOffer(offer_sdp, options);

	Response res;
	HttpPost(live_url.c_str(), offer_sdp, res, 10);
	if (res.code < 200 || res.code >= 300) {
		log_error("Error querying publishing websocket url");
		log_error("code: %d", res.code);
		log_error("body: %s", res.body.c_str());
		// Disconnect, this will call stop on main thread
		obs_output_signal_stop(output_, OBS_OUTPUT_ERROR);
		reset();
		return;
	}

	int32_t ret = peer_connection_->SetRemoteDescription(res.body);
	if (ret != 0) {
		log_error(
			"SetRemoteDescription error ret:%d,\r\n res.body %s\r\n",
			ret, res.body.c_str());
		obs_output_signal_stop(output_, OBS_OUTPUT_ERROR);
		reset();
		return;
	}
	startUdpDownloadThread();
}

bool peer_connection::IsPeerTimeOut()
{
	if ((is_ice_break_ || is_peer_break_)) {
		obs_output_set_last_error(output_, "Connection failure\n\n");
		// Disconnect, this will call stop on main thread
		obs_output_signal_stop(output_, OBS_OUTPUT_ERROR);
		reset();
		return true;
	}
	return false;
}

bool peer_connection::startUdpDownloadThread()
{
	if (m_pUdpProxy) {
		log_info("m_pUdpProxy start download thread\n");
		return (m_pUdpProxy->Start());
	}
	return false;
}

void peer_connection::stopUdpDownloadThread()
{
	if (m_pUdpProxy) {
		m_pUdpProxy->Stop();
	}
}

void peer_connection::OnRecv(const char *pData, uint32_t uDataLen,
			     sockaddr_in &addr)
{
	std::unique_ptr<webrtccore::Packet> recv_pkg(
		new udp_packet((char *)pData, uDataLen));

	webrtccore::NetAddr remote_addr;
	remote_addr.ip = inet_ntoa(addr.sin_addr);
	remote_addr.addr_type = webrtccore::kAddrIpv4;
	remote_addr.port = addr.sin_port;
	if (peer_connection_) {
		peer_connection_->ProcessRemoteData(std::move(recv_pkg),
						    remote_addr);
	}
}

void peer_connection::SetObsOutput(obs_output_t *output)
{
	output_ = output;
}

obs_output_t *peer_connection::GetObsOutput() const
{
	return output_;
}

uint32_t peer_connection::GetLocalVideoSsrc() const
{
	return local_video_ssrc_;
}

uint16_t peer_connection::GetLocalVideoPayloadType() const
{
	return local_video_payloadtype_;
}

uint32_t peer_connection::GetLocalAudioSsrc() const
{
	return local_audio_ssrc_;
}

uint16_t peer_connection::GetLocalAudioPayloadType() const
{
	return local_audio_payloadtype_;
}

uint64_t peer_connection::GetTotalBytesSent() const
{
	return total_bytes_sent_;
}

void peer_connection::AddSendBytes(obs_encoder_type type, int len)
{
	total_bytes_sent_ += len;
}

void peer_connection::reset()
{
	MutexLock lock(&mutex_);
	if (m_pUdpProxy) {
		m_pUdpProxy->Stop();
		delete m_pUdpProxy;
		m_pUdpProxy = nullptr;
	}
	if (peer_connection_) {
		delete peer_connection_;
		peer_connection_ = nullptr;
	}
	m_pUdpProxy = new RtcUdpProxy(this);
	is_ice_break_ = false;
	is_peer_break_ = false;
	total_bytes_sent_ = 0;
}

pthread_mutex_t *peer_connection::GetMute()
{
	return &mutex_;
}

bool &peer_connection::IsRequestKeyFrame() {
	return request_key_frame_;
}
