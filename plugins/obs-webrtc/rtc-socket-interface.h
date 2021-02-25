/*
 *  rtc_socket_interface.h
 *
 *  Created by dengzhao on 2020-6-8.
 *  Copyright 2020 tencent. All rights reserved.
 *
 */
#pragma once

#ifndef WIN32
#include <unistd.h>
#endif
#if defined(ANDROID)
#include <poll.h>
#include <sys/epoll.h>
#include <sys/types.h>
#elif TARGET_OS_MAC
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#include <pthread.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <stdint.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <map>
#include <list>
#include <atomic>
#include <cmath>
#include <mutex>
#include <memory>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>

static const uint32_t kUdpDataLenDefault = 2048;

class IUdpSocketSink {
public:
	virtual ~IUdpSocketSink() {}
	// virtual void OnBind(bool bSuccess, uint32_t uBindIP, uint16_t uBindPort,
	//                     IUdpSocket* pUdpSocket) = 0;
	// virtual void OnRecv(const char* pData, uint32_t uDataLen, uint32_t uFromIP,
	//                     uint16_t uFromPort, IUdpSocket* pUdpSocket) = 0;
	virtual void OnRecv(const char *pData, uint32_t uDataLen,
			    sockaddr_in &addr) = 0;

	// to do some periodical check and action, such as SendBingPacket
	virtual void OnTime() = 0;

	// to check whether peer is timeout
	virtual bool IsPeerTimeOut() = 0;
};

class IUdpSocketSink6 {
public:
	virtual ~IUdpSocketSink6() {}
	// virtual void OnBind(bool bSuccess, const std::string& strBindIP, uint16_t uBindPort,
	//                     IUdpSocket* pUdpSocket) = 0;
	virtual void OnRecv(const char *pData, uint32_t uDataLen,
			    const std::string &strFromIP,
			    uint16_t uFromPort) = 0;
};

class IUdpSocket {
public:
	virtual ~IUdpSocket() {}
	virtual int Getfd() = 0;
	virtual bool Create(uint32_t uBindIP = 0, uint16_t uBindPort = 0) = 0;
	virtual bool Create(const std::string &strBindIP,
			    uint16_t uBindPort = 0,
			    int32_t family = AF_UNSPEC) = 0;
	// virtual void SetSink(IUdpSocketSink* pSink) = 0;
	// virtual void SetSink(IUdpSocketSink6* pSink) = 0;

	virtual int32_t SendTo(const char *ip, uint16_t wPort,
			       const char *pData, uint32_t uBufLen) = 0;
	virtual int32_t SendTo(uint32_t uIP, uint16_t wPort, const char *pData,
			       uint32_t uBufLen) = 0;
	virtual int32_t RecvFrom(char *pData, uint32_t ulen, uint32_t &uFromIP,
				 uint16_t &uFromPort) = 0;
	virtual int32_t RecvFrom(char *pData, uint32_t ulen,
				 std::string &uFromIP, uint16_t &uFromPort) = 0;

	virtual bool SetSendBufferSize(uint32_t size) = 0;
	virtual bool SetRecvBufferSize(uint32_t size) = 0;
	virtual void Close() = 0;

	virtual bool GetSocketName6(std::string &strIP, uint16_t &port) = 0;
};

/**
 * @brief  RtcUdpProxy
 */
class RtcUdpProxy {
public:
	explicit RtcUdpProxy(IUdpSocketSink *sink);
	virtual ~RtcUdpProxy();

	void SendTo(const char *ip, uint16_t uPort, const char *data,
		    uint32_t len);
	void SendTo(uint32_t uIP, uint16_t uPort, const char *pData,
		    uint32_t len);

	bool Start();
	void Stop();
	/**
     * @brief  工作线程，用于下载数据，可以被stop()函数中断。
     *
     * @param  :  void*
     */
	static void *DownloadProc(void *);

private:
	pthread_t GetThreadId();

	pthread_t tid_;
	bool is_joinable_ = false;
	std::atomic<bool> to_be_stoped_;
	IUdpSocketSink *sink_ = nullptr;
	IUdpSocket *udp_socket_ = nullptr;
	std::mutex mutex_;
};
