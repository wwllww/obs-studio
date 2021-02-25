/*****************************************************************************
 * @copyright Copyright (C), 1998-2020, Tencent Tech. Co., Ltd.
 * @brief    the proxy for rtc udp socket class, abstract apis to use rtc
 *           udp socket.
 * @author   dengzhao
 * @version  1.0.0
 * @date     2020/6/17
 * @license  GNU General Public License (GPL)
 *****************************************************************************/

#include "rtc-socket-interface.h"
#include "rtc-udp-socket.h"
#include "webrtc-core.h"


RtcUdpProxy::RtcUdpProxy(IUdpSocketSink* sink) : tid_() {
    sink_ = sink;
    udp_socket_ = new RtcUdpSocket();
}

RtcUdpProxy::~RtcUdpProxy() {
    if (udp_socket_) {
        delete udp_socket_;
        udp_socket_ = nullptr;
    }
}

void* RtcUdpProxy::DownloadProc(void* pParam) {
    RtcUdpProxy* proxy = reinterpret_cast<RtcUdpProxy*>(pParam);
    if (proxy) {
        int sockfd = proxy->udp_socket_->Getfd();
        log_info("DownloadProc in, sockfd:%d", sockfd);
        int ret = 0;
        while (!proxy->to_be_stoped_) {
            fd_set read_set, write_set;
            FD_ZERO(&read_set);
            FD_ZERO(&write_set);
            FD_SET(sockfd, &read_set);
            struct timeval t = {0, 10000};
            ret = select(sockfd + 1, &read_set, &write_set, NULL, &t);
            if (-1 == ret) {
		    log_error("ERROR: DownloadProc failed to select sock.");
		    proxy->is_joinable_ = false;
		    return NULL;
            }
            if (FD_ISSET(sockfd, &read_set)) {
                while (!proxy->to_be_stoped_) {
                    // TODO(dengzhao): replace RecvFrom to RecvFrom6 to adapt with ipv6
                    struct sockaddr_in remote_addr;
                    uint32_t ip;
                    uint16_t port;
                    uint32_t len = kUdpDataLenDefault;
                    char* recv_buf = new char[len];
                    int32_t pkg_len = proxy->udp_socket_->RecvFrom(recv_buf, len, ip, port);

                    if (-1 == pkg_len && (EAGAIN == errno || EWOULDBLOCK == errno)) {
                        delete[] recv_buf;
                        break;
                    }
                    if (pkg_len < 0) {
                        delete[] recv_buf;
                        break;
                    }

                    remote_addr.sin_family = PF_INET;
                    remote_addr.sin_addr.s_addr = ip;
                    remote_addr.sin_port = htons(port);
                    log_debug("recv data from ip:%s port:%d pkg_len:%d",
                         inet_ntoa(remote_addr.sin_addr),
                         ntohs(remote_addr.sin_port), pkg_len);
                    if (proxy->sink_) {
                        // callback to hand over downloaded data
                        proxy->sink_->OnRecv(recv_buf, pkg_len, remote_addr);
                    } else {
                        delete[] recv_buf;
                    }
                }
            }
            if (proxy->sink_) {
                proxy->sink_->OnTime();
                if (proxy->sink_->IsPeerTimeOut()) {
                    break;
                }
            }
        }
    }
    proxy->is_joinable_ = false;
    log_info("DownloadProc out");
    return NULL;
}

void RtcUdpProxy::SendTo(uint32_t uIP, uint16_t uPort, const char* pData, uint32_t len) {
    if (udp_socket_) {
        udp_socket_->SendTo(uIP, uPort, pData, len);
    }
}

void RtcUdpProxy::SendTo(const char* ip, uint16_t uPort, const char* data, uint32_t len) {
    if (udp_socket_) {
        udp_socket_->SendTo(ip, uPort, data, len);
    }
}

bool RtcUdpProxy::Start() {
    int result = -1;
    std::lock_guard<std::mutex> lock(mutex_);
    to_be_stoped_.store(false);
    if (!is_joinable_) {
        result = pthread_create(&tid_, 0, DownloadProc, reinterpret_cast<void*>(this));
        if (0 == result) {
            log_info("pthread_create success.");
            is_joinable_ = true;
        } else {
            log_error("pthread_create failed.");
            is_joinable_ = false;
            return false;
        }
    } else {
        log_info("RtcUdpProxy thread already started, is_joinable_:%d", is_joinable_);
        return false;
    }

    return true;
}

pthread_t RtcUdpProxy::GetThreadId() { return tid_; }

void RtcUdpProxy::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    log_info("stop in");
    if (is_joinable_) {
        is_joinable_ = false;
        to_be_stoped_.store(true);
        log_info("try to pthread_join");
        pthread_join(GetThreadId(), nullptr);
        log_info("pthread_join done");
        if (udp_socket_) {
            udp_socket_->Close();
        }
    }
}
