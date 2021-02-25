/*
 *  rtc_udp_socket.cpp
 *
 *  Created by dengzhao on 2020-6-8.
 *  Copyright 2020 tencent. All rights reserved.
 *
 */

#include "rtc-udp-socket.h"

#include <string>
#include "webrtc-core.h"
#include "rtc-socket-interface.h"


RtcUdpSocket::RtcUdpSocket() {
    bool success = Create();
    if (!success) {
        log_error("create socket fail.\n");
    } else {
        log_info("create socket success.\n");
    }
}

RtcUdpSocket::~RtcUdpSocket() { Close(); }

bool RtcUdpSocket::RecoverSocket() {
    int family = RtcnetIsIpv4(bind_ip_.c_str()) ? AF_INET : AF_INET6;
    RtcSocket s = RtcSocketCreate6(false, false, family);
    if (!RtcSocketIsValid(s)) {
        return false;
    }

    // TODO(dengzhao): need bind or not ?
    // if (!RtcSocketBind6(s, bind_ip_, bind_port_)) {
    //     RtcSocketClose(s);
    //     return false;
    // }

    std::string bind_ip;
    uint16_t bind_port = 0;

    if (!RtcSocketGetSockName6(s, bind_ip, bind_port)) {
        RtcSocketClose(s);
        return false;
    }

    socket_ = s;
    uint32_t nsendbufsize = 0;
    if (RtcSocketGetSendBufSize(s, nsendbufsize) && nsendbufsize < kUdpDataLenDefault) {
        SetSendBufferSize(kUdpDataLenDefault);
        RtcSocketGetSendBufSize(s, nsendbufsize);
    }

    uint32_t nrecvbufsize = 0;
    if (RtcSocketGetRecvBufSize(s, nrecvbufsize) && nrecvbufsize < kUdpDataLenDefault) {
        SetRecvBufferSize(kUdpDataLenDefault);
        RtcSocketGetRecvBufSize(s, nrecvbufsize);
    }

    log_info("Udp RecoverSocket success port[%d] !!", bind_port_);

    return true;
}

bool RtcUdpSocket::Create() {
    socket_ = RtcSocketCreate(false, false);
    if (!RtcSocketIsValid(socket_)) {
        return false;
    }
    return true;
}

bool RtcUdpSocket::Create(uint32_t uBindIP, uint16_t uBindPort) {
    char bind_ip_buf[INET_ADDRSTRLEN] = {0};
    struct in_addr bind_addr = {0};
    bind_addr.s_addr = uBindIP;
    RtcnetInetNtop(AF_INET, reinterpret_cast<void*>(&bind_addr), bind_ip_buf, sizeof(bind_ip_buf));
    return this->Create(bind_ip_buf, uBindPort, AF_INET);
}

bool RtcUdpSocket::Create(const std::string& strBindIP, uint16_t uBindPort, int family) {
    if (family == AF_UNSPEC) {
        family = RtcnetIsIpv4(strBindIP.c_str()) ? AF_INET : AF_INET6;
    }

    RtcSocket s = RtcSocketCreate6(false, false, family);
    if (!RtcSocketIsValid(s)) {
        log_error("RtcUdpSocket invalid socket.");
        return false;
    }

    if (!RtcSocketBind6(s, strBindIP.c_str(), uBindPort)) {
        log_error("RtcUdpSocket bind failed");
        RtcSocketClose(s);
        return false;
    }

    std::string bind_ip;
    uint16_t bind_port = 0;
    if (!RtcSocketGetSockName6(s, bind_ip, bind_port)) {
        RtcSocketClose(s);
        return false;
    }

    bind_ip_ = bind_ip;
    bind_port_ = bind_port;
    socket_ = s;

    if (bind_ip.size() == 0 || RtcnetIsInaddrAny(bind_ip.c_str())) {
        // TODO(dengzhao): to be fixed by dengzhao

        // std::string localip;

        // if (family == AF_INET) {
        //     localip = rtcnet_get_default_localip4();
        //     if (localip.size() == 0 || RtcnetIsInaddrAny(localip.c_str())) {
        //         localip = rtcnet_get_default_localip6();
        //     }
        // } else {
        //     localip = rtcnet_get_default_localip6();
        //     if (localip.size() == 0 || RtcnetIsInaddrAny(localip.c_str())) {
        //         localip = rtcnet_get_default_localip4();
        //     }
        // }

        // bind_ip_.assign(localip);
    }

    uint32_t nsendbufsize = 0;
    if (RtcSocketGetSendBufSize(s, nsendbufsize) && nsendbufsize < kUdpDataLenDefault) {
        SetSendBufferSize(kUdpDataLenDefault);
        RtcSocketGetSendBufSize(s, nsendbufsize);
    }

    uint32_t nrecvbufsize = 0;
    if (RtcSocketGetRecvBufSize(s, nrecvbufsize) && nrecvbufsize < kUdpDataLenDefault) {
        SetRecvBufferSize(kUdpDataLenDefault);
        RtcSocketGetRecvBufSize(s, nrecvbufsize);
    }

     log_info("Udp Create sendbufsize[%d] recvbufsize[%d] port[%d]", nsendbufsize,
                     nrecvbufsize, uBindPort);

    // this->CallSinkOnBind(true, bind_ip_, bind_port);
    return true;
}

int RtcUdpSocket::Getfd() { return socket_.socketfd; }

// void RtcUdpSocket::SetSink(IUdpSocketSink* pSink) {
//     m_pSink6 = nullptr;
//     sink_ = pSink;
// }

// void RtcUdpSocket::SetSink(IUdpSocketSink6* pSink6) {
//     sink_ = nullptr;
//     m_pSink6 = pSink6;
// }

int32_t RtcUdpSocket::SendTo(const char* ip, uint16_t uPort, const char* pData, uint32_t uBufLen) {
    return RtcSocketSendTo6(socket_, pData, uBufLen, ip, uPort);
}

int32_t RtcUdpSocket::SendTo(uint32_t uIP, uint16_t uPort, const char* pData, uint32_t uBufLen) {
    return RtcSocketSendTo(socket_, pData, uBufLen, uIP, uPort);
}

int32_t RtcUdpSocket::RecvFrom(char* pData, uint32_t ulen, uint32_t& uFromIP, uint16_t& uFromPort) {
    return RtcSocketRecvFrom(socket_, pData, ulen, uFromIP, uFromPort);
}

int32_t RtcUdpSocket::RecvFrom(char* pData, uint32_t ulen, std::string& uFromIP,
                               uint16_t& uFromPort) {
    char from_ip_buffer[INET6_ADDRSTRLEN] = {0};
    uint16_t from_port = 0;
    int r = RtcSocketRecvFrom6(socket_, pData, ulen, from_ip_buffer, sizeof(from_ip_buffer),
                               &from_port);
    uFromIP.assign(from_ip_buffer);
    uFromPort = from_port;
    return r;
}

bool RtcUdpSocket::SetSendBufferSize(uint32_t size) {
    return RtcSocketSetSendBufSize(socket_, size);
}

bool RtcUdpSocket::SetRecvBufferSize(uint32_t size) {
    return RtcSocketSetRecvBufSize(socket_, size);
}

bool RtcUdpSocket::GetSocketName(uint32_t& ip, uint16_t& port) {
    return RtcSocketGetSockName(socket_, ip, port);
}

bool RtcUdpSocket::GetSocketName6(std::string& strIP, uint16_t& port) {
    return RtcSocketGetSockName6(socket_, strIP, port);
}

void RtcUdpSocket::Close() {
    if (RtcSocketIsValid(socket_)) {
        RtcSocketClose(socket_);
        socket_.family = PF_UNSPEC;
        socket_.socketfd = kInvalidSocket;
    }
}
