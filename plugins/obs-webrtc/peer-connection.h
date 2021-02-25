#ifndef PEERCONNETC_H
#define PEERCONNETC_H
#include "webrtc-core.h"

class MutexLock {
public:
	MutexLock(const MutexLock &) = delete;
	MutexLock &operator=(const MutexLock &) = delete;

	explicit MutexLock(pthread_mutex_t *mutex) : mutex_(mutex)
	{
		pthread_mutex_lock(mutex_);
	}
	~MutexLock() { pthread_mutex_unlock(mutex_); }

private:
	pthread_mutex_t *mutex_;
};

typedef std::map<std::string, std::string> HeaderFields;

/** @struct Response
  *  @brief This structure represents the HTTP response data
  *  @var Response::code
  *  Member 'code' contains the HTTP response code
  *  @var Response::body
  *  Member 'body' contains the HTTP response body
  *  @var Response::headers
  *  Member 'headers' contains the HTTP response headers
  */
typedef struct {
	int code;
	std::string body;
	HeaderFields headers;
} Response;

class udp_packet : public webrtccore::Packet {
public:
	udp_packet(char *data, int32_t len)
	{
		data_ = data;
		len_ = len;
	}
	~udp_packet()
	{
		if (data_) {
			delete[] data_;
			data_ = NULL;
		}
	};
};

class MediaFrame : public webrtccore::MeidaData {
public:
	MediaFrame(webrtccore::MediaType media_type, char *data, int32_t len,
		   uint64_t timestamp, uint32_t ssrc, uint32_t payload_type,
		   webrtccore::VideoRotation rotate_angle,
		   uint32_t audio_frame_len)
	{
		media_type_ = media_type;
		len_ = len;
		timestamp_ = timestamp;
		ssrc_ = ssrc;
		rotate_angle_ = rotate_angle;
		payload_type_ = payload_type;
		audio_frame_len_ = audio_frame_len;

		if (0 < len) {
			data_ = new char[len];
			memcpy(data_, data, len);
		} else {
			data_ = nullptr;
		}
	}
	virtual ~MediaFrame()
	{
		if (data_) {
			delete[] data_;
		}
	}
};

class peer_connection : public webrtccore::PeerConnectionObserver,
			public IUdpSocketSink {
public:
	peer_connection();
	~peer_connection();

protected:
	/*call back*/
	void OnIceConnectionChange(webrtccore::IceConnectionState new_state);
	void OnConnectionChange(webrtccore::PeerConnectionState new_state);
	void OnAddAudioTrack(uint32_t ssrc, const std::string &label,
			     const std::map<uint32_t, webrtccore::AudioCodeInfo>
				     &codec_info_map);
	void OnRemoveAudioTrack(uint32_t ssrc);
	void OnAddVideoTrack(uint32_t ssrc, const std::string &label,
			     const std::map<uint32_t, webrtccore::VideoCodeInfo>
				     &codec_info_map);
	void OnRemoveVideoTrack(uint32_t ssrc);
	void OnRecvMeidaData(std::unique_ptr<webrtccore::MeidaData> media_data);
	void OnRecvChannelData(std::unique_ptr<webrtccore::Packet> data);
	void OnSendDataToRemote(std::unique_ptr<webrtccore::Packet> data,
				const webrtccore::NetAddr &addr);
	void OnRequestIFrame(uint32_t ssrc);
	void OnSendDumpData(char *dump_data, int32_t len);
	void
	OnAddLocalAudioTrack(uint32_t ssrc, const std::string &label,
			     const std::map<uint32_t, webrtccore::AudioCodeInfo>
				     &codec_info_map);

	void
	OnAddLocalVideoTrack(uint32_t ssrc, const std::string &label,
			     const std::map<uint32_t, webrtccore::VideoCodeInfo>
				     &codec_info_map);

	/* work*/
	void UdpSocketSend(char *data, int32_t len,
			   const webrtccore::NetAddr &addr);
	CURLcode HttpPost(const std::string &url, std::string str_json,
			  Response &res, int time_out);

public:
	webrtccore::PeerConnectionInterface *GetPC();
	void SetPC(webrtccore::PeerConnectionInterface *pc);
	virtual void OnTime();
	void SdpExchange(const std::string &stream_id,
			 const std::string &live_url);
	virtual bool IsPeerTimeOut();
	bool startUdpDownloadThread();
	void stopUdpDownloadThread();
	virtual void OnRecv(const char *pData, uint32_t uDataLen,
			    sockaddr_in &addr);
	void SetObsOutput(obs_output_t *output);
	obs_output_t *GetObsOutput() const;
	uint32_t GetLocalVideoSsrc() const;
	uint16_t GetLocalVideoPayloadType() const;
	uint32_t GetLocalAudioSsrc() const;
	uint16_t GetLocalAudioPayloadType() const;
	uint64_t GetTotalBytesSent() const;
	void AddSendBytes(obs_encoder_type type, int len);
	void reset();
	pthread_mutex_t *GetMute();
	bool &IsRequestKeyFrame();

private:
	bool is_ice_break_;
	bool is_peer_break_;
	webrtccore::PeerConnectionInterface *peer_connection_ = nullptr;
	RtcUdpProxy *m_pUdpProxy = nullptr;

	uint32_t local_video_ssrc_ = 0;
	uint16_t local_video_payloadtype_ = 0;
	uint32_t local_audio_ssrc_ = 0;
	uint16_t local_audio_payloadtype_ = 0;
	std::map<uint32_t, webrtccore::VideoCodeInfo>
		local_video_codec_info_map_;
	std::map<uint32_t, webrtccore::AudioCodeInfo>
		local_audio_codec_info_map_;

	obs_output_t *output_;
	pthread_mutex_t mutex_;
	uint64_t total_bytes_sent_ = 0;
	bool request_key_frame_ = false;
	uint64_t last_request_key_frame_time_ = 0;
};

#endif
