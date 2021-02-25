/*****************************************************************************
 * @copyright Copyright (C), 1998-2020, Tencent Tech. Co., Ltd.
 * @brief    the proxy for rtc udp socket class, abstract apis to use rtc
 *           udp socket.
 * @author   dengzhao
 * @version  1.0.0
 * @date     2020/6/17
 * @license  GNU General Public License (GPL)
 *****************************************************************************/

#include "rtc-net-utils.h"

#include <string.h>

#include "common-net-utils.h"
#ifdef _OS_ANDROID_
#include <linux/if.h>
#include <sys/select.h>
#else
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma warning(disable:4100)
#else
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include <cstdio>
#include <string>
#include "webrtc-core.h"

#ifndef __cplusplus
#define false 0
#define true 1
#endif

const int32_t kInvalidSocket = -1;
static const uint32_t kMaxLoopCount = 10;
static int32_t kIPstackInUse = kRtcnetIpstackNone;

static int rtc_test_connect(int pf, struct sockaddr *addr, uint32_t addrlen,
			    struct sockaddr *local_addr,
			    socklen_t local_addr_len)
{
	int s = socket(pf, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		return 0;
	int ret;
	uint32_t loopcount = 0;
	do {
		ret = connect(s, addr, addrlen);
	} while (ret < 0 && errno == EINTR && loopcount++ < kMaxLoopCount);
	if (loopcount >= kMaxLoopCount) {
		log_error("rtc_test_connect connect error. loopcount = %d", loopcount);
	}
	int success = (ret == 0);
	if (success) {
		memset(local_addr, 0, sizeof(struct sockaddr_storage));
		ret = getsockname(s, local_addr, &local_addr_len);
		if (0 != ret) {
			log_error("rtc_test_connect getsockname failed.");
		}
	}
	loopcount = 0;
	do {
#ifdef _WIN32
		ret = closesocket(s);
#else
		ret = close(s);
#endif
	} while (ret < 0 && errno == EINTR && loopcount++ < kMaxLoopCount);

	if (loopcount >= kMaxLoopCount) {
		log_error("rtc_test_connect close error. loopcount = %d", loopcount);
	}

	return success;
}

/*
 * The following functions determine whether IPv4 or IPv6 connectivity is
 * available in order to implement AI_ADDRCONFIG.
 *
 * Strictly speaking, AI_ADDRCONFIG should not look at whether connectivity is
 * available, but whether addresses of the specified family are "configured
 * on the local system". However, bionic doesn't currently support getifaddrs,
 * so checking for connectivity is the next best thing.
 */
static int rtc_have_ipv6(struct sockaddr *local_addr, socklen_t local_addr_len)
{
#ifdef __APPLE__
	static const struct sockaddr_in6 sin6_test = {
		.sin6_len = sizeof(sin6_test),
		.sin6_family = AF_INET6,
		.sin6_port = 80,
		.sin6_flowinfo = 0,
		.sin6_scope_id = 0,
		.sin6_addr.s6_addr = {// 2000::
				      0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				      0, 0, 0}};
	SockAddrUnion addr = {.in6 = sin6_test};
#else
	static struct sockaddr_in6 sin6_test;
	sin6_test.sin6_family = AF_INET6;
	sin6_test.sin6_port = 80;
	sin6_test.sin6_flowinfo = 0;
	sin6_test.sin6_scope_id = 0;
	memset(sin6_test.sin6_addr.s6_addr, 0,
	       sizeof(sin6_test.sin6_addr.s6_addr));
	sin6_test.sin6_addr.s6_addr[0] = 0x20;
	SockAddrUnion addr;
	addr.in6 = sin6_test;
#endif
	return rtc_test_connect(PF_INET6, &addr.generic, sizeof(addr.in6),
				local_addr, local_addr_len);
}

static int rtc_have_ipv4(struct sockaddr *local_addr, socklen_t local_addr_len)
{
#ifdef __APPLE__
	static const struct sockaddr_in sin_test = {
		.sin_len = sizeof(sin_test),
		.sin_family = AF_INET,
		.sin_port = 80,
		.sin_addr.s_addr = htonl(0x08080808L), // 8.8.8.8
	};
	SockAddrUnion addr = {.in = sin_test};
#else
	static struct sockaddr_in sin_test;
	sin_test.sin_family = AF_INET;
	sin_test.sin_port = 80;

	sin_test.sin_addr.s_addr = htonl(0x08080808L); // 8.8.8.8
	SockAddrUnion addr;
	addr.in = sin_test;
#endif
	return rtc_test_connect(PF_INET, &addr.generic, sizeof(addr.in),
				local_addr, local_addr_len);
}

static int32_t local_ipstack_detect()
{
	sockaddr_storage v4_addr;
	sockaddr_storage v6_addr;
	memset(&v4_addr, 0, sizeof(v4_addr));
	memset(&v6_addr, 0, sizeof(v6_addr));
	int haveIpv4 = rtc_have_ipv4(reinterpret_cast<sockaddr *>(&v4_addr),
				     sizeof(v4_addr));
	int haveIpv6 = rtc_have_ipv6(reinterpret_cast<sockaddr *>(&v6_addr),
				     sizeof(v6_addr));

	int32_t localStack = kRtcnetIpstackNone;

	if (haveIpv4) {
		localStack |= kRtcnetIpstackIpv4;
	}
	if (haveIpv6) {
		localStack |= kRtcnetIpstackIpv6;
	}

	log_info("local ipstack detect: haveIpv4 %d, haveIpv6 %d", haveIpv4, haveIpv6);
	kIPstackInUse = localStack;
	return kIPstackInUse;
}

static int rtcsocket_getfamily(RtcSocket s)
{
	return s.family;
}

int32_t RtcnetGetIpstack()
{
	if (kIPstackInUse != kRtcnetIpstackNone) {
		if (kIPstackInUse == kRtcnetIpstackIpv4 ||
		    kIPstackInUse == kRtcnetIpstackIpv6 ||
		    kIPstackInUse == kRtcnetIpstackDual) {
			return kIPstackInUse;
		}
	}

	int32_t ipstack = local_ipstack_detect();
	return ipstack;
}

static bool rtcsocket_should_mapv4tov6(RtcSocket s)
{
	int family = rtcsocket_getfamily(s);
	if (family != AF_UNSPEC) {
		return family == AF_INET6;
	} else {
		int32_t ipstack = RtcnetGetIpstack();
		return (ipstack == kRtcnetIpstackIpv6 ||
			ipstack == kRtcnetIpstackDual);
	}
}

RtcSocket RtcSocketCreate6(bool istcp, bool reuse, int family)
{
	RtcSocket s = {AF_UNSPEC, kInvalidSocket};

	if (istcp) {
		s.family = family;
		s.socketfd = socket(family, SOCK_STREAM, IPPROTO_TCP);
	} else {
		s.family = family;
		s.socketfd = socket(family, SOCK_DGRAM, 0);
	}

	log_info("RtcSocket create fd: %d %d", s.socketfd, errno);

	if (kInvalidSocket == s.socketfd) {
		return s;
	}

	// set non-blocking mode
	bool setnonblock = false;
#ifdef _WIN32
	u_long mode = 1;
	setnonblock = (0 == ioctlsocket(s.socketfd, FIONBIO, &mode));
#else
	int32_t flags = fcntl(s.socketfd, F_GETFL, 0);
	setnonblock = (-1 != fcntl(s.socketfd, F_SETFL, flags | O_NONBLOCK));
#endif

	if (!setnonblock) {
		RtcSocketClose(s);
		s.family = AF_UNSPEC;
		s.socketfd = kInvalidSocket;
		return s;
	}

	int ret = 0;
#ifdef __APPLE__
	int set = 1;
	ret = setsockopt(s.socketfd, SOL_SOCKET, SO_NOSIGPIPE,
			 reinterpret_cast<void *>(&set), sizeof(int));

	if (-1 == ret) {
		log_error("setsockopt failed, line:%d.", __LINE__);
	}

#endif

	if (reuse) {
#ifdef _OS_ANDROID_
#else
		int optval = 1;
		ret = setsockopt(s.socketfd, SOL_SOCKET, SO_REUSEADDR,
				 reinterpret_cast<char *>(&optval),
				 sizeof(optval)); // comfirmed by others

		if (-1 == ret) {
			log_error("setsockopt failed, line:%d.", __LINE__);
		}

#if defined(SO_REUSEPORT)
		int optval2 = 1;
		ret = setsockopt(s.socketfd, SOL_SOCKET, SO_REUSEPORT,
				 reinterpret_cast<char *>(&optval2),
				 sizeof(optval2));

		if (-1 == ret) {
			log_error("setsockopt failed, line:%d.",
					__LINE__);
		}

#endif // defined(SO_REUSEPORT)
#endif // _OS_ANDROID_
	}

	if (!istcp) {
		int optval = 1;
		ret = setsockopt(s.socketfd, SOL_SOCKET, SO_BROADCAST,
				 reinterpret_cast<char *>(&optval),
				 sizeof(optval));

		if (-1 == ret) {
			log_error("setsockopt failed, line:%d.", __LINE__);
		}
	}

	if (s.socketfd > 1023) {
		log_error("RtcSocketCreate error: RtcSocket is overFlow :%d ", s.socketfd);
	}

	return s;
}

RtcSocket RtcSocketCreate(bool istcp, bool reuse)
{
	int family = PF_INET;
	int local_ip_stack = RtcnetGetIpstack();

	if (kRtcnetIpstackIpv6 == local_ip_stack) {
		family = PF_INET6;
	} else {
		family = PF_INET;
	}
	log_info("RtcSocketCreate istcp:%d isipv6:%d RtcnetGetIpstack():%d", istcp,
	                     family == PF_INET6 ? 1 : 0, RtcnetGetIpstack());

	return RtcSocketCreate6(istcp, reuse, family);
}

bool RtcSocketClose(RtcSocket s)
{
	if (!RtcSocketIsValid(s)) {
		return false;
	}
	int ret = 0;
#ifdef _WIN32
	ret = closesocket(s.socketfd);
#else
	ret = close(s.socketfd);
#endif
	return (0 == ret);
}

bool RtcSocketIsValid(RtcSocket s)
{
	return (s.socketfd != kInvalidSocket);
}

bool RtcSocketSetSendBufSize(RtcSocket s, uint32_t size)
{
	int32_t rv = setsockopt(s.socketfd, SOL_SOCKET, SO_SNDBUF,
				reinterpret_cast<const char *>(&size),
				sizeof(size));
	return (rv == 0);
}

bool RtcSocketSetRecvBufSize(RtcSocket s, uint32_t size)
{
	int32_t rv = setsockopt(s.socketfd, SOL_SOCKET, SO_RCVBUF,
				reinterpret_cast<const char *>(&size),
				sizeof(size));
	return rv == 0;
}

bool RtcSocketGetSendBufSize(RtcSocket s, uint32_t &size)
{
	socklen_t l = sizeof(l);
	int32_t rv = getsockopt(s.socketfd, SOL_SOCKET, SO_SNDBUF,
				reinterpret_cast<char *>(&size), &l);
	return rv == 0;
}

bool RtcSocketGetRecvBufSize(RtcSocket s, uint32_t &size)
{
	socklen_t l = sizeof(l);
	int32_t rv = getsockopt(s.socketfd, SOL_SOCKET, SO_RCVBUF,
				reinterpret_cast<char *>(&size), &l);
	return rv == 0;
}

bool RtcSocketSetNoDelay(RtcSocket s, bool bistoset)
{
	int32_t kDisableNagle = bistoset ? 1 : 0;

	int rv = setsockopt(s.socketfd, IPPROTO_TCP, TCP_NODELAY,
			    reinterpret_cast<const char *>(&kDisableNagle),
			    sizeof(kDisableNagle));
	return rv == 0;
}

bool RtcSocketSetMulticastTtl(RtcSocket s, int ttl)
{
	bool rc = false;
#if defined(_OS_MAC_)
	unsigned char ucttl = ttl & 0xFF;
	int rv = setsockopt(s.socketfd, IPPROTO_IP, IP_MULTICAST_TTL,
			    reinterpret_cast<char *>(&ucttl), sizeof(ucttl));
	rc = (rv == 0);
#endif
	return rc;
}

bool RtcSocketJoinGroup(RtcSocket s, const char *mcastAddr, const char *ifAddr)
{
	bool ret = true;

#if defined(_OS_MAC_)
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

	struct addrinfo *mcastAddrInfo, *ifAddrInfo;
	if (getaddrinfo(mcastAddr, NULL, &hints, &mcastAddrInfo) != 0)
		return false;
	if (getaddrinfo(ifAddr, NULL, &hints, &ifAddrInfo) != 0) {
		freeaddrinfo(mcastAddrInfo);
		return false;
	}

	int retCode;

	struct ip_mreq ipmr;
	struct sockaddr_in toaddr, ifaddr;
	memcpy(&toaddr, mcastAddrInfo->ai_addr, sizeof(struct sockaddr_in));
	memcpy(&ifaddr, ifAddrInfo->ai_addr, sizeof(struct sockaddr_in));
	memcpy(&ipmr.imr_multiaddr.s_addr, &toaddr.sin_addr,
	       sizeof(struct in_addr));
	memcpy(&ipmr.imr_interface.s_addr, &ifaddr.sin_addr,
	       sizeof(struct in_addr));

	retCode =
		setsockopt(s.socketfd, IPPROTO_IP, IP_MULTICAST_IF,
			   reinterpret_cast<char *>(&ipmr.imr_interface.s_addr),
			   sizeof(struct in_addr));
	if (retCode != 0) {
		ret = false;
	}
	retCode = setsockopt(s.socketfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			     reinterpret_cast<char *>(&ipmr), sizeof(ipmr));
	if (retCode != 0) {
		ret = false;
	}

	freeaddrinfo(mcastAddrInfo);
	freeaddrinfo(ifAddrInfo);

#endif
	return ret;
}

// socket tcp
int32_t RtcSocketSend(RtcSocket s, const void *buf, uint32_t len)
{
	if (0 == buf || len <= 0 || !RtcSocketIsValid(s)) {
		return 0;
	}
	int32_t r = (int32_t)send(s.socketfd, (const char *)buf, len, 0);

	return r;
}

int32_t RtcSocketRecv(RtcSocket s, void *buf, uint32_t len)
{
	if (0 == buf || len <= 0 || !RtcSocketIsValid(s)) {
		return 0;
	}
	int32_t r = (int32_t)recv(s.socketfd, reinterpret_cast<char *>(buf),
				  len, 0);
	return r;
}

uint32_t RtcSocketGetUnreadDataLen(RtcSocket s)
{
	if (!RtcSocketIsValid(s)) {
		return 0;
	}

	uint32_t count = 0;
#ifdef _WIN32
	u_long lcount = 0;
	ioctlsocket(s.socketfd, FIONREAD, &lcount);
	count = lcount;
#else
	ioctl(s.socketfd, FIONREAD, &count);
#endif
	return count;
}

bool RtcSocketBind6(RtcSocket s, const char *ip, uint16_t port)
{
	struct sockaddr_storage sock_addr = {0};
	socklen_t sockaddr_len = sizeof(sock_addr);

	// bind ipv4 address
	if (RtcnetIsIpv4(ip)) {
		uint32_t u32ip = 0;
		if (ip && strlen(ip) > 0) {
			u32ip = RtcnetStrToIp(ip);
		}

		bool mapv4tov6 = rtcsocket_should_mapv4tov6(s);
		// v6only environment, try convert to nat64 address
		if (mapv4tov6) {
			struct sockaddr_in6 *v6_addr =
				reinterpret_cast<struct sockaddr_in6 *>(
					&sock_addr);
			v6_addr->sin6_family = AF_INET6;
#if defined(_OS_MAC_)
			v6_addr->sin6_len = sizeof(sock_addr);
#endif
			v6_addr->sin6_port = htons(port);
			sockaddr_len = sizeof(sockaddr_in6);
			if (u32ip == INADDR_ANY) {
				in6_addr ipv6_addr = {0};
				v6_addr->sin6_addr = ipv6_addr;
			} else if (u32ip == INADDR_LOOPBACK) {
				in6_addr ipv6_addr = IN6ADDR_LOOPBACK_INIT;
				v6_addr->sin6_addr = ipv6_addr;
			} else {
				log_error("RtcSocketBind6 cannot bind %s:%u on ipv6only env.", ip, port);
				return false;
			}
		} else {
			// dual stack or v4 only environment, just go with old school ipv4
			struct sockaddr_in *v4_addr =
				reinterpret_cast<struct sockaddr_in *>(
					&sock_addr);
			v4_addr->sin_family = AF_INET;
			v4_addr->sin_port = htons(port);
			memcpy(&v4_addr->sin_addr,
			       reinterpret_cast<void *>(&u32ip), sizeof(u32ip));
			sockaddr_len = sizeof(sockaddr_in);
		}
	} else {
		// ipv6 address, go with ipv6
		in6_addr ipv6_addr = {0};
		RtcnetStrToIpv6(ip, &ipv6_addr);
		struct sockaddr_in6 *v6_addr =
			reinterpret_cast<struct sockaddr_in6 *>(&sock_addr);
		v6_addr->sin6_family = AF_INET6;
		v6_addr->sin6_port = htons(port);
		v6_addr->sin6_addr = ipv6_addr;
		sockaddr_len = sizeof(sockaddr_in6);
	}

	if (bind(s.socketfd, (struct sockaddr *)&sock_addr, sockaddr_len) < 0) {
		return false;
	} else {
		return true;
	}
}

bool RtcSocketBind(RtcSocket s, int32_t ip, uint16_t port)
{
	bool mapv4tov6 = rtcsocket_should_mapv4tov6(s);

	struct sockaddr_storage sock_addr = {0};
	socklen_t sockaddr_len = sizeof(sock_addr);
	if (mapv4tov6) {
		struct sockaddr_in6 *v6_addr =
			reinterpret_cast<struct sockaddr_in6 *>(&sock_addr);
		v6_addr->sin6_family = AF_INET6;
#if defined(_OS_MAC_)
		v6_addr->sin6_len = sizeof(sock_addr);
#endif
		v6_addr->sin6_port = htons(port);
		sockaddr_len = sizeof(sockaddr_in6);
		if (ip == INADDR_ANY) {
			in6_addr ipv6_addr = {0};
			v6_addr->sin6_addr = ipv6_addr;
		} else if (ip == INADDR_LOOPBACK) {
			in6_addr ipv6_addr = IN6ADDR_LOOPBACK_INIT;
			v6_addr->sin6_addr = ipv6_addr;
		} else {
			log_error("RtcSocketBind cannot bind %d:%u on ipv6only env.", ip, port);
			return false;
		}
	} else {
		// dual stack or v4 only, bind ipv4 ip, just old school ipv4
		struct sockaddr_in *addr =
			reinterpret_cast<struct sockaddr_in *>(&sock_addr);
#if defined(_OS_MAC_)
		addr->sin_len = sizeof(sock_addr);
#endif
		addr->sin_family = AF_INET;
		addr->sin_port = htons(port);
		addr->sin_addr.s_addr = ip;
		sockaddr_len = sizeof(sockaddr_in);
	}

	if (bind(s.socketfd, (struct sockaddr *)&sock_addr, sockaddr_len) < 0) {
		return false;
	}

	return true;
}

bool RtcSocketGetPeerName(RtcSocket s, uint32_t &ip, uint16_t &port)
{
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);

	if (0 == getpeername(s.socketfd,
			     reinterpret_cast<struct sockaddr *>(&sa), &len)) {
		ip = sa.sin_addr.s_addr;
		port = ntohs(sa.sin_port);
		return true;
	}

	return false;
}

bool rtcsocket_getpeername6(RtcSocket s, std::string &ip, uint16_t &port)
{
	struct sockaddr_storage sa;
	socklen_t len = sizeof(sa);

	if (0 == getpeername(s.socketfd,
			     reinterpret_cast<struct sockaddr *>(&sa), &len)) {
		char str[INET6_ADDRSTRLEN] = {0};
		RtcnetIpToStr6(&sa, str, sizeof(str));
		ip = str;
		int family =
			(reinterpret_cast<struct sockaddr *>(&sa))->sa_family;
		if (family == PF_INET) {
			port = ntohs(
				(reinterpret_cast<struct sockaddr_in *>(&sa))
					->sin_port);
		} else if (family == PF_INET6) {
			port = ntohs(
				(reinterpret_cast<struct sockaddr_in6 *>(&sa))
					->sin6_port);
		}
		return true;
	}

	return false;
}

bool RtcSocketGetSockName(RtcSocket s, uint32_t &ip, uint16_t &port)
{
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);

	if (0 == getsockname(s.socketfd,
			     reinterpret_cast<struct sockaddr *>(&sa), &len)) {
		ip = sa.sin_addr.s_addr;
		port = ntohs(sa.sin_port);
		return true;
	}

	return false;
}

bool RtcSocketGetSockName6(RtcSocket s, std::string &ip, uint16_t &port)
{
	struct sockaddr_storage sa;
	socklen_t len = sizeof(sa);

	if (0 == getsockname(s.socketfd,
			     reinterpret_cast<struct sockaddr *>(&sa), &len)) {
		char str[INET6_ADDRSTRLEN] = {0};
		RtcnetIpToStr6(&sa, str, sizeof(str));
		ip = str;
		int family =
			(reinterpret_cast<struct sockaddr *>(&sa))->sa_family;
		if (family == PF_INET) {
			port = ntohs(
				(reinterpret_cast<struct sockaddr_in *>(&sa))
					->sin_port);
		} else if (family == PF_INET6) {
			port = ntohs(
				(reinterpret_cast<struct sockaddr_in6 *>(&sa))
					->sin6_port);
		}
		return true;
	}

	return false;
}

// socket udp
int32_t RtcSocketRecvFrom6(RtcSocket s, void *buf, uint32_t len, char *fromip,
			   uint32_t fromiplen, uint16_t *fromport)
{
	int32_t r = 0;

	if (!RtcSocketIsValid(s) || nullptr == fromip || 0 == fromiplen) {
		return 0;
	}

	struct sockaddr_storage sock_addr = {0};
	socklen_t addrlen = sizeof(sock_addr);
	r = (int32_t)recvfrom(s.socketfd, static_cast<char *>(buf), len, 0,
			      reinterpret_cast<struct sockaddr *>(&sock_addr),
			      &addrlen);

	if (r > 0) {
		int family = sock_addr.ss_family;
		if (family == PF_INET6) {
			struct sockaddr_in6 *v6_addr =
				reinterpret_cast<struct sockaddr_in6 *>(
					&sock_addr);
			RtcnetInetNtop(PF_INET6, &(v6_addr->sin6_addr), fromip,
				       fromiplen);
			*fromport = ntohs(v6_addr->sin6_port);
		} else if (family == PF_INET) {
			struct sockaddr_in *v4_addr =
				reinterpret_cast<struct sockaddr_in *>(
					&sock_addr);
			RtcnetInetNtop(PF_INET, &(v4_addr->sin_addr), fromip,
				       fromiplen);
			*fromport = ntohs(v4_addr->sin_port);
		} else {
			 log_error("RtcSocketRecvFrom6 recved %d but addr family is unknown: %d", r,
					family);
			return 0;
		}
	}

	return r;
}

int32_t RtcSocketRecvFrom(RtcSocket s, void *buf, uint32_t len,
			  uint32_t &fromip, uint16_t &fromport)
{
	int32_t r = 0;

	if (!RtcSocketIsValid(s) || 0 == buf || 0 == len) {
		return 0;
	}

	struct sockaddr_storage sock_addr = {0};
	socklen_t addrlen = sizeof(sock_addr);
	r = (int32_t)recvfrom(s.socketfd, reinterpret_cast<char *>(buf), len, 0,
			      reinterpret_cast<struct sockaddr *>(&sock_addr),
			      &addrlen);

	if (r > 0) {
		int family = sock_addr.ss_family;
		if (family == PF_INET6) {
			struct sockaddr_in6 *v6_addr =
				reinterpret_cast<struct sockaddr_in6 *>(
					&sock_addr);
			fromport = ntohs(v6_addr->sin6_port);

			uint8_t *addr_ptr = reinterpret_cast<uint8_t *>(
				&(v6_addr->sin6_addr));

			fromip = *(reinterpret_cast<uint32_t *>(addr_ptr + 12));
		} else if (family == PF_INET) {
			struct sockaddr_in *v4_addr =
				reinterpret_cast<struct sockaddr_in *>(
					&sock_addr);
			fromip = v4_addr->sin_addr.s_addr;
			fromport = ntohs(v4_addr->sin_port);
		} else {
			log_error("RtcSocketRecvFrom recved %d but addr family is unknown: %d", r,
			                family);
			return 0;
		}
	}

	return r;
}

int32_t RtcSocketSendTo6(RtcSocket s, const void *buf, uint32_t len,
			 const char *addr, uint16_t port)
{
	if (!RtcSocketIsValid(s) || len == 0 || buf == NULL || addr == NULL ||
	    port == 0) {
		return 0;
	}
	int32_t r = -1;
	struct sockaddr_storage sock_addr = {0};
	socklen_t sockaddr_len = sizeof(sock_addr);
	// ipv4 address
	if (RtcnetIsIpv4(addr)) {
		bool mapv4tov6 = rtcsocket_should_mapv4tov6(s);
		// v6only environment, try convert to nat64 address
		if (mapv4tov6) {
			char ipv6Buf[INET6_ADDRSTRLEN];
			bool success = RtcnetSynthesizeNat64Ipv6(
				addr, ipv6Buf, INET6_ADDRSTRLEN);
			if (!success) {
				success = RtcnetSynthesizeV4MappedIpv6(
					addr, ipv6Buf, INET6_ADDRSTRLEN);
				if (!success) {
					return false;
				}
			}
			struct in6_addr ipv6_addr = {0};
			RtcnetStrToIpv6(ipv6Buf, &ipv6_addr);
			struct sockaddr_in6 *v6_addr =
				reinterpret_cast<struct sockaddr_in6 *>(
					&sock_addr);
			v6_addr->sin6_family = AF_INET6;
			v6_addr->sin6_port = htons(port);
			v6_addr->sin6_addr = ipv6_addr;
			sockaddr_len = sizeof(sockaddr_in6);
		} else {
			// dual stack or v4 only environment, just go with old school ipv4
			// target ipv4
			uint32_t u32ip = RtcnetStrToIp(addr);
			struct sockaddr_in *v4_addr =
				reinterpret_cast<struct sockaddr_in *>(
					&sock_addr);
			v4_addr->sin_family = AF_INET;
			v4_addr->sin_port = htons(port);
			memcpy(&v4_addr->sin_addr,
			       reinterpret_cast<void *>(&u32ip), sizeof(u32ip));
			sockaddr_len = sizeof(sockaddr_in);
		}
	} else {
		// ipv6 address, go with ipv6
		in6_addr ipv6_addr = {0};
		RtcnetStrToIpv6(addr, &ipv6_addr);
		struct sockaddr_in6 *v6_addr =
			reinterpret_cast<struct sockaddr_in6 *>(&sock_addr);
		v6_addr->sin6_family = AF_INET6;
		v6_addr->sin6_port = htons(port);
		v6_addr->sin6_addr = ipv6_addr;
		sockaddr_len = sizeof(sockaddr_in6);
	}

	r = (int32_t)sendto(s.socketfd, (const char *)buf, len, 0,
			    reinterpret_cast<struct sockaddr *>(&sock_addr),
			    sockaddr_len);
	return r;
}

int32_t RtcSocketSendTo(RtcSocket s, const void *buf, uint32_t len,
			uint32_t uAddr, uint16_t port)
{
	if (!RtcSocketIsValid(s) || len == 0 || buf == NULL || uAddr == 0 ||
	    port == 0) {
		return 0;
	}
	int32_t r = -1;
	const char *addr = RtcnetIpToStr(uAddr);

	bool mapv4tov6 = rtcsocket_should_mapv4tov6(s);
	if (mapv4tov6) {
		char ipv6Buf[INET6_ADDRSTRLEN];
		bool success = RtcnetSynthesizeNat64Ipv6(addr, ipv6Buf,
							 INET6_ADDRSTRLEN);
		if (!success) {
			success = RtcnetSynthesizeV4MappedIpv6(
				addr, ipv6Buf, INET6_ADDRSTRLEN);
			if (!success) {
				return -1;
			}
		}
		in6_addr ipv6_addr = {0};
		RtcnetStrToIpv6(ipv6Buf, &ipv6_addr);
		sockaddr_in6 v6_addr = {0};
		v6_addr.sin6_family = AF_INET6;
		v6_addr.sin6_port = htons(port);
		v6_addr.sin6_addr = ipv6_addr;
		r = (int32_t)sendto(s.socketfd, (const char *)buf, len, 0,
				    (struct sockaddr *)&v6_addr,
				    sizeof(v6_addr));
	} else {
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
#if defined(_OS_MAC_)
		sin.sin_len = sizeof(sin);
#endif
		sin.sin_addr.s_addr = uAddr;
		sin.sin_port = htons(port);
		sin.sin_family = AF_INET;

		r = (int32_t)sendto(s.socketfd, (const char *)buf, len, 0,
				    reinterpret_cast<struct sockaddr *>(&sin),
				    sizeof(sin));
	}

	return r;
}
