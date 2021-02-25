// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: qq@tencent.com (Emperor Penguin)

#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h>
#include <stdint.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#endif

#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#ifdef _OS_ANDROID_
#include <linux/if.h>
#include <sys/select.h>
#else
#endif // _OS_ANDROID_

#ifdef __cplusplus
extern "C" {
#endif

// If ipstr is an ipv4 address(x.x.x.x), it will return the uin32 ipv4 address.
// If ipstr is an ipv6 address, it will convert it to in_addr6,
// and return the last 4 bytes of it.
int32_t RtcNetExtractIpv4FromString(const char *ipstr, uint32_t *ipv4);

// rtcnet ipaddress helpers
int32_t RtcnetInetNtop4(const unsigned char *src, char *dst, int32_t size);
int32_t RtcnetInetNtop6(const unsigned char *src, char *dst, int32_t size);
int32_t RtcnetInetPton4(const char *src, unsigned char *dst);
int32_t RtcnetInetPton6(const char *src, unsigned char *dst);

int32_t RtcnetIp4Addr(const char *ip, uint16_t port, struct sockaddr_in *addr);
int32_t RtcnetIp6Addr(const char *ip, uint16_t port, struct sockaddr_in6 *addr);

int32_t RtcnetIp4Name(const struct sockaddr_in *src, char *dst, int32_t size);
int32_t RtcnetIp6Name(const struct sockaddr_in6 *src, char *dst, int32_t size);

int32_t RtcnetInetNtop(int32_t af, const void *src, char *dst, int32_t size);
int32_t RtcnetInetPton(int32_t af, const char *src, void *dst);

bool RtcnetIsInaddrAny(const char *ipstr);

typedef struct RtcnetIpaddress {
	union {
		struct sockaddr_in addr4;
		struct sockaddr_in6 addr6;
		struct sockaddr_storage addr;
	};
} RtcnetIpaddress;

RtcnetIpaddress RtcnetIpaddressFromStr(const char *ipstr);

bool RtcnetIpaddressIsvalid(const RtcnetIpaddress *ipaddr);
const char *RtcnetIpToStr6(struct sockaddr_storage *addr, char *dst, int size);
bool RtcnetIsIpv4(const char *ip);
int32_t RtcnetStrToIpv6(const char *ipAddress, struct in6_addr *sAddr6);
char *RtcnetIpToStr(uint32_t ip);
uint32_t RtcnetStrToIp(const char *ip);

// using system's getaddrinfo function to transfer ipv4 address to ipv6 address
bool SynthesizeIpv6(const char *ipv4, char *ipv6, uint32_t ipv6len);
bool RtcnetIpv4toIpv6(const char *ipv4, char *ipv6, uint32_t ipv6len);
bool RtcnetSynthesizeNat64Ipv6(const char *ipv4, char *ipv6, uint32_t ipv6len);
bool RtcnetSynthesizeV4MappedIpv6(const char *ipv4, char *ipv6,
				  uint32_t ipv6len);

#ifdef __cplusplus
}
#endif
