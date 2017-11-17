#ifndef KAYCC_NET_ENDIAN_H
#define KAYCC_NET_ENDIAN_H

#include <stdint.h>
#include <endian.h>

#include <arpa/inet.h> //htonl

namespace kaycc {
namespace net {
namespace sockets {

	//glibc2.9支持htobe 和betoh系列函数
	// 主机字节顺序的uint64_t转换为网络字节顺序的uint64_t 
	inline uint64_t hostToNetwork64(uint64_t host64) {
		return htobe64(host64); //本地字节序转大端字节序64
	}

	inline uint32_t hostToNetwork32(uint32_t host32) {
		return htobe32(host32);
	}

	inline uint16_t hostToNetwork16(uint16_t host16) {
		return htobe16(host16);
	}

	// 网络字节顺序的uint64_t转换为主机字节顺序的uint64_t
	inline uint64_t networkToHost64(uint64_t host64) {
		return be64toh(host64);//大端64转本地字节序
	}
	
	inline uint32_t networkToHost32(uint64_t host32) {
		return be32toh(host32);
	}

	inline uint16_t networkToHost16(uint64_t host16) {
		return be16toh(host16);
	}

	//本地字节序int64转网络字节序
	uint64_t htonll(uint64_t v)
	{
    	union
    	{
        	uint32_t lv[2];
        	uint64_t llv;
    	} u;
    	u.lv[0] = htonl(v >> 32); //网络字节序的低32位是本地字节序的高32位，v>>32得到本地高32位
    	u.lv[1] = htonl(v & 0xFFFFFFFFULL);//v & FFFFFFFFULL 得到本地低32位
    	return u.llv;
	}

	//网络字节序int64转本地字节序
	int64_t ntohll(uint64_t v)
	{
    	union
    	{
        	uint32_t lv[2];
        	uint64_t llv;
    	} u;
    	u.llv = v;
    	return ((uint64_t)ntohl(u.lv[0]) << 32) | (uint64_t)ntohl(u.lv[1]);
	}
} //end sockets
} //end net
}

#endif
