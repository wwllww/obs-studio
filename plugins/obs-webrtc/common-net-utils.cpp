// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: qq@tencent.com (Emperor Penguin)

#include "common-net-utils.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WS2tcpip.h>
#include <stdint.h>
#include <Iphlpapi.h>
#else
#include <net/if.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <string.h>
#include "webrtc-core.h"


#define RTC_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#ifndef AI_DEFAULT
#define AI_DEFAULT (AI_V4MAPPED | AI_ADDRCONFIG)
#endif


RtcnetIpaddress RtcnetIpaddressFromStr(const char* ipstr) {
    struct RtcnetIpaddress ipaddress;
    int r = RtcnetIp4Addr(ipstr, 0, &(ipaddress.addr4));
    if (r == 0) {
        return ipaddress;
    }

    r = RtcnetIp6Addr(ipstr, 0, &(ipaddress.addr6));
    if (r == 0) {
        return ipaddress;
    }

    ipaddress.addr.ss_family = -1;
    return ipaddress;
}

bool RtcnetIpaddressIsvalid(const RtcnetIpaddress* ipaddr) {
    return ipaddr && (ipaddr->addr.ss_family == AF_INET || ipaddr->addr.ss_family == AF_INET6);
}

bool RtcnetIsInaddrAny(const char* ipstr) {
    static uint32_t any4 = INADDR_ANY;
    static struct in6_addr any6 = IN6ADDR_ANY_INIT;

    if (ipstr) {
        RtcnetIpaddress ipaddress;
        memset(&ipaddress, 0, sizeof(ipaddress));
        ipaddress = RtcnetIpaddressFromStr(ipstr);

        if (RtcnetIpaddressIsvalid(&ipaddress)) {
            if (ipaddress.addr.ss_family == AF_INET) {
                return memcmp(&ipaddress.addr4.sin_addr, &any4, sizeof(any4)) == 0;
            } else if (ipaddress.addr.ss_family == AF_INET6) {
                return memcmp(&ipaddress.addr6.sin6_addr, &any6, sizeof(any6)) == 0;
            }
        }
    }

    return false;
}

int RtcNetExtractIpv4FromString(const char* ipstr, uint32_t* ipv4) {
    RtcnetIpaddress ipaddress;
    memset(&ipaddress, 0, sizeof(ipaddress));
    ipaddress = RtcnetIpaddressFromStr(ipstr);

    if (!RtcnetIpaddressIsvalid(&ipaddress)) {
        return -1;
    }

    int family = ipaddress.addr.ss_family;
    if (family == AF_INET) {
        const struct in_addr* addr4 = &(ipaddress.addr4.sin_addr);
        if (addr4) {
            *ipv4 = addr4->s_addr;
            return 0;
        }
    } else if (family == AF_INET6) {
        const struct in6_addr* addr6 = &(ipaddress.addr6.sin6_addr);
        if (addr6) {
            memcpy(ipv4, addr6->s6_addr + 12, sizeof(uint32_t));
            return 0;
        }
    }

    return -1;
}

int32_t RtcnetIp4Addr(const char* ip, uint16_t port, struct sockaddr_in* addr) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    return RtcnetInetPton(AF_INET, ip, &(addr->sin_addr.s_addr));
}

int32_t RtcnetIp6Addr(const char* ip, uint16_t port, struct sockaddr_in6* addr) {
    char address_part[40];
    uint32_t address_part_size;
    const char* zone_index;

    memset(addr, 0, sizeof(*addr));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);

    zone_index = strchr(ip, '%');
    if (zone_index != NULL) {
        address_part_size = zone_index - ip;
        if (address_part_size >= sizeof(address_part)) address_part_size = sizeof(address_part) - 1;

        memcpy(address_part, ip, address_part_size);
        address_part[address_part_size] = '\0';
        ip = address_part;

        zone_index++; /* skip '%' */
                      /* NOTE: unknown int32_terface (id=0) is silently ignored */
        addr->sin6_scope_id = if_nametoindex(zone_index);
    }

    return RtcnetInetPton(AF_INET6, ip, &addr->sin6_addr);
}

int32_t RtcnetIp4Name(const struct sockaddr_in* src, char* dst, int32_t size) {
    return RtcnetInetNtop(AF_INET, &src->sin_addr, dst, size);
}

int32_t RtcnetIp6Name(const struct sockaddr_in6* src, char* dst, int32_t size) {
    return RtcnetInetNtop(AF_INET6, &src->sin6_addr, dst, size);
}

int32_t RtcnetInetNtop(int32_t af, const void* src, char* dst, int32_t size) {
    switch (af) {
        case AF_INET:
            return (RtcnetInetNtop4((const unsigned char*)src, dst, size));
        case AF_INET6:
            return (RtcnetInetNtop6((const unsigned char*)src, dst, size));
        default:
            return -1;
    }
}

int32_t RtcnetInetPton(int32_t af, const char* src, void* dst) {
    if (src == NULL || dst == NULL) return -1;

    switch (af) {
        case AF_INET:
            return (RtcnetInetPton4(src, (unsigned char*)dst));
        case AF_INET6: {
            int32_t len;
            char tmp[INET6_ADDRSTRLEN], *s;
            s = const_cast<char *>(src);
            const char* p = strchr(src, '%');
            if (p != NULL) {
                s = tmp;
                len = p - src;
                if (len > INET6_ADDRSTRLEN - 1) return -1;
                memcpy(s, src, len);
                s[len] = '\0';
            }
            return RtcnetInetPton6(s, (unsigned char*)dst);
        }
        default:
            return -1;
    }
}

int32_t rtc_strscpy(char* d, const char* s, uint32_t n) {
    uint32_t i;

    for (i = 0; i < n; i++)
        if ('\0' == (d[i] = s[i])) return ((i > INT32_MAX) ? -1 : static_cast<int32_t>(i));

    if (i == 0) return 0;

    d[--i] = '\0';
    return -1;
}

int32_t RtcnetInetNtop4(const unsigned char* src, char* dst, int32_t size) {
    static const char fmt[] = "%u.%u.%u.%u";
    char tmp[INET_ADDRSTRLEN];
    int32_t l;

    l = snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]);
    if (l <= 0 || (int32_t)l >= size) {
        return -1;
    }

    rtc_strscpy(dst, tmp, size);
    return -1;
}

int32_t RtcnetInetNtop6(const unsigned char* src, char* dst, int32_t size) {
    /*
     * Note that int32_t and int16_t need only be "at least" large enough
     * to contain a value of the specified size.  On some systems, like
     * Crays, there is no such thing as an int32_teger variable with 16 bits.
     * Keep this in mind if you think this function should have been coded
     * to use pointer overlays.  All the world's not a VAX.
     */
    char tmp[INET6_ADDRSTRLEN], *tp;
    struct {
        int32_t base, len;
    } best, cur;
    int32_t words[sizeof(struct in6_addr) / sizeof(uint16_t)];
    int32_t i;
    /*
     * Preprocess:
     *  Copy the input (bytewise) array int32_to a wordwise array.
     *  Find the longest run of 0x00's in src[] for :: shorthanding.
     */
    memset(words, '\0', sizeof words);
    for (i = 0; i < (int32_t)sizeof(struct in6_addr); i++)
        words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
    best.base = -1;
    best.len = 0;
    cur.base = -1;
    cur.len = 0;
    for (i = 0; i < (int32_t)RTC_ARRAY_SIZE(words); i++) {
        if (words[i] == 0) {
            if (cur.base == -1)
                cur.base = i, cur.len = 1;
            else
                cur.len++;
        } else {
            if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len) best = cur;
                cur.base = -1;
            }
        }
    }
    if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len) best = cur;
    }
    if (best.base != -1 && best.len < 2) best.base = -1;

    /*
     * Format the result.
     */
    tp = tmp;
    for (i = 0; i < (int32_t)RTC_ARRAY_SIZE(words); i++) {
        /* Are we inside the best run of 0x00's? */
        if (best.base != -1 && i >= best.base && i < (best.base + best.len)) {
            if (i == best.base) *tp++ = ':';
            continue;
        }
        /* Are we following an initial run of 0x00s or any real hex? */
        if (i != 0) *tp++ = ':';
        /* Is this address an encapsulated IPv4? */
        if (i == 6 && best.base == 0 &&
            (best.len == 6 || (best.len == 7 && words[7] != 0x0001) ||
             (best.len == 5 && words[5] == 0xffff))) {
            int32_t err = RtcnetInetNtop4(src + 12, tp, sizeof tmp - (tp - tmp));
            if (err) return err;
            tp += strlen(tp);
            break;
        }
        tp += snprintf(tp, sizeof(tmp) - (tp - tmp), "%x", words[i]);
    }
    /* Was it a trailing run of 0x00's? */
    if (best.base != -1 && (best.base + best.len) == RTC_ARRAY_SIZE(words)) *tp++ = ':';
    *tp++ = '\0';
    if (-1 == rtc_strscpy(dst, tmp, size)) return -1;
    return 0;
}

int32_t RtcnetInetPton4(const char* src, unsigned char* dst) {
    static const char digits[] = "0123456789";
    int32_t saw_digit, octets, ch;
    unsigned char tmp[sizeof(struct in_addr)], *tp;

    saw_digit = 0;
    octets = 0;
    *(tp = tmp) = 0;
    while ((ch = *src++) != '\0') {
        const char* pch;

        if ((pch = strchr(digits, ch)) != NULL) {
            int32_t nw = *tp * 10 + (pch - digits);

            if (saw_digit && *tp == 0) return -1;
            if (nw > 255) return -1;
            *tp = nw;
            if (!saw_digit) {
                if (++octets > 4) return -1;
                saw_digit = 1;
            }
        } else if (ch == '.' && saw_digit) {
            if (octets == 4) return -1;
            *++tp = 0;
            saw_digit = 0;
        } else {
            return -1;
        }
    }
    if (octets < 4) return -1;
    memcpy(dst, tmp, sizeof(struct in_addr));
    return 0;
}

int32_t RtcnetInetPton6(const char* src, unsigned char* dst) {
    static const char xdigits_l[] = "0123456789abcdef", xdigits_u[] = "0123456789ABCDEF";
    unsigned char tmp[sizeof(struct in6_addr)], *tp, *endp, *colonp;
    const char *xdigits, *curtok;
    int32_t ch, seen_xdigits;
    int32_t val;

    memset((tp = tmp), '\0', sizeof tmp);
    endp = tp + sizeof tmp;
    colonp = NULL;
    /* Leading :: requires some special handling. */
    if (*src == ':')
        if (*++src != ':') return -1;
    curtok = src;
    seen_xdigits = 0;
    val = 0;
    while ((ch = *src++) != '\0') {
        const char* pch;

        if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
            pch = strchr((xdigits = xdigits_u), ch);
        if (pch != NULL) {
            val <<= 4;
            val |= (pch - xdigits);
            if (++seen_xdigits > 4) return -1;
            continue;
        }
        if (ch == ':') {
            curtok = src;
            if (!seen_xdigits) {
                if (colonp) return -1;
                colonp = tp;
                continue;
            } else if (*src == '\0') {
                return -1;
            }
            if (tp + sizeof(uint16_t) > endp) return -1;
            *tp++ = (unsigned char)(val >> 8) & 0xff;
            *tp++ = (unsigned char)val & 0xff;
            seen_xdigits = 0;
            val = 0;
            continue;
        }
        if (ch == '.' && ((tp + sizeof(struct in_addr)) <= endp)) {
            int32_t err = RtcnetInetPton4(curtok, tp);
            if (err == 0) {
                tp += sizeof(struct in_addr);
                seen_xdigits = 0;
                break; /*%< '\\0' was seen by inet_pton4(). */
            }
        }
        return -1;
    }
    if (seen_xdigits) {
        if (tp + sizeof(uint16_t) > endp) return -1;
        *tp++ = (unsigned char)(val >> 8) & 0xff;
        *tp++ = (unsigned char)val & 0xff;
    }
    if (colonp != NULL) {
        /*
         * Since some memmove()'s erroneously fail to handle
         * overlapping regions, we'll do the shift by hand.
         */
        const int32_t n = tp - colonp;
        int32_t i;

        if (tp == endp) return -1;
        for (i = 1; i <= n; i++) {
            endp[-i] = colonp[n - i];
            colonp[n - i] = 0;
        }
        tp = endp;
    }
    if (tp != endp) return -1;
    memcpy(dst, tmp, sizeof tmp);
    return 0;
}

const char* RtcnetIpToStr6(struct sockaddr_storage* addr, char* dst, int size) {
    void* src = NULL;
    if (addr->ss_family == PF_INET) {
        src = &((reinterpret_cast<sockaddr_in*>(addr))->sin_addr);
    } else if (addr->ss_family == PF_INET6) {
        src = &((reinterpret_cast<sockaddr_in6*>(addr))->sin6_addr);
    }
    if (src == NULL) {
        return const_cast<char*>("");
    }
    RtcnetInetNtop(addr->ss_family, src, dst, size);
    return dst;
}

bool RtcnetIsIpv4(const char* ip) {
    if (!ip || strlen(ip) == 0) {
        return false;
    }

    RtcnetIpaddress ipaddress;
    memset(&ipaddress, 0, sizeof(ipaddress));

    int r = RtcnetIp4Addr(ip, 0, &ipaddress.addr4);
    if (r == 0) {
        return true;
    } else {
        return false;
    }
}

int RtcnetStrToIpv6(const char* ipAddress, struct in6_addr* sAddr6) {
    return RtcnetInetPton(AF_INET6, ipAddress, reinterpret_cast<void*>(sAddr6));
}

char* RtcnetIpToStr(uint32_t ip) { return inet_ntoa(*(struct in_addr*)&ip); }

uint32_t RtcnetStrToIp(const char* ip) {
    if (!ip) return INADDR_NONE;
    return inet_addr((const char*)ip);
}

// using system's getaddrinfo function to transfer ipv4 address to ipv6 address
bool SynthesizeIpv6(const char* ipv4, char* ipv6, uint32_t ipv6len) {
    if (ipv4 == NULL || ipv6 == NULL || ipv6len < INET6_ADDRSTRLEN) {
        return false;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family =
        PF_UNSPEC;  // unspecified family，os will try best to return ipv6 or ipv4 address.
    hints.ai_socktype = SOCK_STREAM;

#ifdef _OS_ANDROID_
    hints.ai_flags = 0;
#else
    hints.ai_flags = AI_DEFAULT;
#endif
    struct addrinfo* res = NULL;
    int error = getaddrinfo(ipv4, "http", &hints, &res);

    bool success = false;
    if (error) {
#ifdef _WIN32
        int e = WSAGetLastError();
        log_info("rtcnet_synthesize_ipv6 getaddrinfo error: errorno %d e: ", error, e);
#else
        log_info("rtcnet_synthesize_ipv6 getaddrinfo error: errorno %d e: %s ", error,
                        gai_strerror(error));
#endif
        success = false;
    } else {
        for (struct addrinfo* r = res; r; r = r->ai_next) {
            if (res->ai_family == AF_INET6) {
                struct sockaddr_in6* addr6 = (struct sockaddr_in6*)res->ai_addr;
                RtcnetInetNtop(AF_INET6, &(addr6->sin6_addr), ipv6, ipv6len);
                success = true;
		log_info("rtcnet_synthesize_ipv6 synthesized an ipv6 -> %s ",
			 ipv6);
                break;
            } else if (res->ai_family == AF_INET) {
		    log_info("rtcnet_synthesize_ipv6 get an ipv4 ip");
            } else {
		    log_info("rtcnet_synthesize_ipv6 get an unknown ai family type");
            }
        }
        freeaddrinfo(res);
    }

    return success;
}

bool RtcnetIpv4toIpv6(const char* ipv4, char* ipv6, uint32_t ipv6len) {
    if (ipv4 == NULL || ipv6 == NULL || ipv6len < INET6_ADDRSTRLEN) {
        return false;
    }
    return SynthesizeIpv6(ipv4, ipv6, ipv6len);
}

bool RtcnetSynthesizeNat64Ipv6(const char* ipv4, char* ipv6, uint32_t ipv6len) {
    return RtcnetIpv4toIpv6(ipv4, ipv6, ipv6len);
}

bool RtcnetSynthesizeV4MappedIpv6(const char* ipv4, char* ipv6, uint32_t ipv6len) {
    if (!ipv4 || !ipv6 || ipv6len < INET6_ADDRSTRLEN) {
        return false;
    }

    static const char* v4mapped_prefix = "::FFFF:";
    const uint32_t v4mapped_prefix_len = strlen(v4mapped_prefix);
    const uint32_t ipv4len = strlen(ipv4);

    if ((v4mapped_prefix_len + ipv4len + 1) > ipv6len) {
        return false;
    }
    strncpy(ipv6, v4mapped_prefix, v4mapped_prefix_len);
    strncpy(ipv6 + v4mapped_prefix_len, ipv4, ipv4len);
    ipv6[v4mapped_prefix_len + ipv4len] = '\0';
    return true;
}
