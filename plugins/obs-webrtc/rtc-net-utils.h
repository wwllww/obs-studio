/*
 *  rtc_net_utils.h
 *
 *  Created by dengzhao on 2020-6-8.
 *  Copyright 2020 tencent. All rights reserved.
 *
 */
#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#include <ws2ipdef.h>
#else
#include <netinet/in.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

extern const int32_t kInvalidSocket;

typedef struct RtcSocket {
	int32_t family;
	int32_t socketfd;
} RtcSocket;

enum IpStack {
	kRtcnetIpstackNone = 0x00,
	kRtcnetIpstackIpv4 = 0x01,
	kRtcnetIpstackIpv6 = 0x02,
	kRtcnetIpstackDual = kRtcnetIpstackIpv6 |
			     kRtcnetIpstackIpv4, // dual == 3
};

typedef union SockAddrUnion {
	struct sockaddr generic;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
} SockAddrUnion;

int32_t RtcnetGetIpstack();
RtcSocket RtcSocketCreate6(bool istcp, bool reuse, int family);
RtcSocket RtcSocketCreate(bool istcp, bool reuse);
bool RtcSocketClose(RtcSocket s);
bool RtcSocketIsValid(RtcSocket s);

bool RtcSocketSetSendBufSize(RtcSocket s, uint32_t size);
bool RtcSocketSetRecvBufSize(RtcSocket s, uint32_t size);
bool RtcSocketGetSendBufSize(RtcSocket s, uint32_t &size);
bool RtcSocketGetRecvBufSize(RtcSocket s, uint32_t &size);
bool RtcSocketSetNoDelay(RtcSocket s, bool bistoset);
bool RtcSocketSetMulticastTtl(RtcSocket s, int ttl);
bool RtcSocketJoinGroup(RtcSocket s, const char *mcastAddr, const char *ifAddr);
uint32_t RtcSocketGetUnreadDataLen(RtcSocket s);

bool RtcSocketGetPeerName(RtcSocket s, uint32_t &ip, uint16_t &port);
bool RtcSocketGetPeerName6(RtcSocket s, std::string &ip, uint16_t &port);
bool RtcSocketGetSockName(RtcSocket s, uint32_t &ip, uint16_t &port);
bool RtcSocketGetSockName6(RtcSocket s, std::string &ip, uint16_t &port);

bool RtcSocketBind6(RtcSocket s, const char *ip, uint16_t port);
bool RtcSocketBind(RtcSocket s, int32_t ip, uint16_t port);

// socket tcp
int32_t RtcSocketSend(RtcSocket s, const void *buf, uint32_t len);
int32_t RtcSocketRecv(RtcSocket s, void *buf, uint32_t len);

// socket udp
int32_t RtcSocketRecvFrom6(RtcSocket s, void *buf, uint32_t len, char *fromip,
			   uint32_t fromiplen, uint16_t *fromport);
int32_t RtcSocketRecvFrom(RtcSocket s, void *buf, uint32_t len,
			  uint32_t &fromip, uint16_t &fromport);
int32_t RtcSocketSendTo6(RtcSocket s, const void *buf, uint32_t len,
			 const char *addr, uint16_t port);
int32_t RtcSocketSendTo(RtcSocket s, const void *buf, uint32_t len,
			uint32_t uAddr, uint16_t port);

#ifdef __cplusplus
}
#endif
