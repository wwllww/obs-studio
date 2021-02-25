/*****************************************************************************
 * @copyright Copyright (C), 1998-2020, Tencent Tech. Co., Ltd.
 * @brief    the proxy for rtc udp socket class, abstract apis to use rtc
 *           udp socket.
 * @author   dengzhao
 * @version  1.0.0
 * @date     2020/6/17
 * @license  GNU General Public License (GPL)
 *****************************************************************************/

#pragma once

#include <string>

#include "common-net-utils.h"
#include "rtc-net-utils.h"
#include "rtc-socket-interface.h"

class RtcUdpSocket : public IUdpSocket {
 public:
    RtcUdpSocket();
    virtual ~RtcUdpSocket();
    virtual int Getfd();
    virtual bool Create();
    virtual bool Create(uint32_t uBindIP, uint16_t uBindPort);
    virtual bool Create(const std::string& strBindIP, uint16_t uBindPort,
                        int32_t family = AF_UNSPEC);
    // virtual void SetSink(IUdpSocketSink* pSink);
    // virtual void SetSink(IUdpSocketSink6* pSink);

    virtual int32_t SendTo(const char* ip, uint16_t wPort, const char* pData, uint32_t uBufLen);
    virtual int32_t SendTo(uint32_t uIP, uint16_t wPort, const char* pData, uint32_t uBufLen);
    virtual int32_t RecvFrom(char* pData, uint32_t ulen, uint32_t& uFromIP, uint16_t& uFromPort);
    virtual int32_t RecvFrom(char* pData, uint32_t ulen, std::string& uFromIP, uint16_t& uFromPort);

    virtual bool SetSendBufferSize(uint32_t size);
    virtual bool SetRecvBufferSize(uint32_t size);
    virtual void Close();

    virtual bool GetSocketName(uint32_t& ip, uint16_t& port);
    virtual bool GetSocketName6(std::string& strIP, uint16_t& port);

 private:
    // ios后台休眠后socket会坏掉，需要自动修复
    bool RecoverSocket();

    void CallSinkOnBind(bool bSuccess, std::string& strBindIP, uint16_t uBindPort);
    void CallSinkOnRecv(const char* pData, uint32_t uDataLen, std::string& strFromIP,
                        uint16_t uFromPort);

 private:
    // IUdpSocketSink* sink_ = nullptr;
    // IUdpSocketSink6* m_pSink6 = nullptr;
    RtcSocket socket_ = {PF_UNSPEC, kInvalidSocket};
    uint32_t recv_buffer_len_ = kUdpDataLenDefault;
    uint16_t bind_port_ = 0;
    std::string bind_ip_ = "";
};
