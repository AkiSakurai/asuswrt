/*
 * Copyright 2014 Trend Micro Incorporated
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software without 
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <linux/if_ether.h>

#include "fw_util.h"
#include "skb_access.h"
#include "forward_config.h"

#ifndef IN6ADDRSZ
#define IN6ADDRSZ       16
#endif

#ifndef INT16SZ
#define INT16SZ         2
#endif

#ifndef INADDRSZ
#define INADDRSZ        4
#endif

static int inet_pton4(const char *src, u_char *dst, int pton);
static int inet_pton6(const char *src, u_char *dst);

int inet_pton(int af, const char *src, void *dst)
{
	switch (af)
	{
	case AF_INET:
		return inet_pton4(src, dst, 1);
	case AF_INET6:
		return inet_pton6(src, dst);
	default:
		return (-1);
	}
	/* NOTREACHED */
}

/* int
 * inet_pton4(src, dst, pton)
 *      when last arg is 0: inet_aton(). with hexadecimal, octal and shorthand.
 *      when last arg is 1: inet_pton(). decimal dotted-quad only.
 * return:
 *      1 if rc' is a valid input, else 0.
 * notice:
 *      does not touch st' unless it's returning 1.
 * author:
 *      Paul Vixie, 1996.
 */
static int inet_pton4(const char *src, u_char *dst, int pton)
{
	u_int val = 0;
	u_int digit = 0;
	int base = 0, n = 0;
	unsigned char c = 0;
	u_int parts[4];
	register u_int *pp = parts;

	c = *src;
	for (;;)
	{
		/*
		 * Collect number up to `.''.
		 * Values are specified as for C:
		 * 0x=hex, 0=octal, isdigit=decimal.
		 */
		if (!isdigit(c))
		{
			return (0);
		}
		val = 0;
		base = 10;
		if (c == '0')
		{
			c = *++src;
			if (c == 'x' || c == 'X')
			{
				base = 16, c = *++src;
			}
			else if (isdigit(c) && c != '9')
			{
				base = 8;
			}
		}
		/* inet_pton() takes decimal only */
		if (pton && base != 10)
		{
			return (0);
		}
		for (;;)
		{
			if (isdigit(c))
			{
				digit = c - '0';
				if (digit >= base)
				{
					break;
				}
				val = (val * base) + digit;
				c = *++src;
			}
			else if (base == 16 && isxdigit(c))
			{
				digit = c + 10 - (islower(c) ? 'a' : 'A');
				if (digit >= 16)
				{
					break;
				}
				val = (val << 4) | digit;
				c = *++src;
			}
			else
			{
				break;
			}
		}
		if (c == '.')
		{
			/*
			 * Internet format:
			 *      a.b.c.d
			 *      a.b.c   (with c treated as 16 bits)
			 *      a.b     (with b treated as 24 bits)
			 *      a       (with a treated as 32 bits)
			 */
			if (pp >= parts + 3)
			{
				return (0);
			}
			*pp++ = val;
			c = *++src;
		}
		else
		{
			break;
		}
	}
	/*
	 * Check for trailing characters.
	 */
	if (c != '\0' && !isspace(c))
	{
		return (0);
	}
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts + 1;
	/* inet_pton() takes dotted-quad only.  it does not take shorthand. */
	if (pton && n != 4)
	{
		return (0);
	}
	switch (n)
	{
	case 0:
		return (0); /* initial nondigit */

	case 1: /* a -- 32 bits */
		break;

	case 2: /* a.b -- 8.24 bits */
		if (parts[0] > 0xff || val > 0xffffff)
		{
			return (0);
		}
		val |= parts[0] << 24;
		break;

	case 3: /* a.b.c -- 8.8.16 bits */
		if ((parts[0] | parts[1]) > 0xff || val > 0xffff)
		{
			return (0);
		}
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 4: /* a.b.c.d -- 8.8.8.8 bits */
		if ((parts[0] | parts[1] | parts[2] | val) > 0xff)
		{
			return (0);
		}
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}
	if (dst)
	{
		val = htonl(val);
		memcpy(dst, &val, INADDRSZ);
	}
	return (1);
}

/* int
 * inet_pton6(src, dst)
 *      convert presentation level address to network order binary form.
 * return:
 *      1 if rc' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *      (1) does not touch st' unless it's returning 1.
 *      (2) :: in a full address is silently ignored.
 * credit:
 *      inspired by Mark Andrews.
 * author:
 *      Paul Vixie, 1996.
 */
static int inet_pton6(const char *src, u_char *dst)
{
	static const char xdigits_l[] = "0123456789abcdef",
		xdigits_u[] = "0123456789ABCDEF";
	u_char tmp[IN6ADDRSZ], *tp = NULL, *endp = NULL, *colonp = NULL;
	const char *xdigits = NULL, *curtok = NULL;
	int ch = 0, saw_xdigit = 0;
	u_int val = 0;

	memset((tp = tmp), '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
	{
		if (*++src != ':')
		{
			return (0);
		}
	}
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while ('\0' != (ch = *src++))
	{
		const char *pch;

		if (NULL == (pch = strchr((xdigits = xdigits_l), ch)))
		{
			pch = strchr((xdigits = xdigits_u), ch);
		}

		if (NULL != pch)
		{
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
			{
				return (0);
			}
			saw_xdigit = 1;
			continue;
		}
		if (':' == ch)
		{
			curtok = src;
			if (!saw_xdigit)
			{
				if (colonp)
				{
					return (0);
				}
				colonp = tp;
				continue;
			}
			else if ('\0' == *src)
			{
				return (0);
			}
			if (tp + INT16SZ > endp)
			{
				return (0);
			}
			*tp++ = (u_char) (val >> 8) & 0xff;
			*tp++ = (u_char) val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if (('.' == ch) && ((tp + INADDRSZ) <= endp) &&
			(inet_pton4(curtok, tp, 1) > 0))
		{
			tp += INADDRSZ;
			saw_xdigit = 0;
			break; /* '\0' was seen by inet_pton4(). */
		}
		return (0);
	}
	if (saw_xdigit)
	{
		if (tp + INT16SZ > endp)
		{
			return (0);
		}
		*tp++ = (u_char) (val >> 8) & 0xff;
		*tp++ = (u_char) val & 0xff;
	}
	if (NULL != colonp)
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		if (tp == endp)
		{
			return (0);
		}
		for (i = 1; i <= n; i++)
		{
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
	{
		return (0);
	}
	memcpy(dst, tmp, IN6ADDRSZ);
	return (1);
}

static const char*
inet_ntop4 (const unsigned char *src, char *dst, socklen_t size)
{
  char tmp[sizeof "255.255.255.255"];
  int len = 0;

  len = sprintf (tmp, "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);
  if (len < 0)
  {
	  return NULL;
  }

  if (len > size)
  {
	  return NULL;
  }

  return strcpy (dst, tmp);
}

static const char*
inet_ntop6 (const unsigned char *src, char *dst, socklen_t size)
{
#define NS_IN6ADDRSZ 	16
#define NS_INT16SZ 		2

	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
	char *tp = NULL;
	struct
	{
		int base, len;
	} best, cur;
	unsigned int words[NS_IN6ADDRSZ / NS_INT16SZ];
	int i = 0;

	/*
	 * Preprocess:
	 *      Copy the input (bytewise) array into a wordwise array.
	 *      Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset (words, '\0', sizeof words);
	for (i = 0; i < NS_IN6ADDRSZ; i += 2)
		words[i / 2] = (src[i] << 8) | src[i + 1];
	best.base = cur.base = -1;
	best.len = cur.len = 0;
	for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
	{
		if (words[i] == 0)
		{
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		}
		else
		{
			if (cur.base != -1)
			{
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1)
	{
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	 tp = tmp;
	 for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
	 {
		 /* Are we inside the best run of 0x00's? */
		 if (best.base != -1 && i >= best.base && i < (best.base + best.len))
		 {
			 if (i == best.base)
				 *tp++ = ':';
			 continue;
		 }
		 /* Are we following an initial run of 0x00s or any real hex? */
		 if (i != 0)
			 *tp++ = ':';
		 /* Is this address an encapsulated IPv4? */
		 if (i == 6 && best.base == 0 &&
				 (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
		 {
			 if (!inet_ntop4 (src + 12, tp, sizeof tmp - (tp - tmp)))
				 return (NULL);
			 tp += strlen (tp);
			 break;
		 }
		 {
			 int len = sprintf (tp, "%x", words[i]);
			 if (len < 0)
				 return NULL;
			 tp += len;
		 }
	 }
	 /* Was it a trailing run of 0x00's? */
	 if (best.base != -1 && (best.base + best.len) ==
			 (NS_IN6ADDRSZ / NS_INT16SZ))
		 *tp++ = ':';
	 *tp++ = '\0';

	 /*
	  * Check for overflow, copy, and we're done.
	  */
	 if ((socklen_t) (tp - tmp) > size)
	 {
		 return NULL;
	 }

	 return strcpy (dst, tmp);
}


/* char *
 * inet_ntop(af, src, dst, size)
 *	convert a network format address to presentation format.
 * return:
 *	pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *	Paul Vixie, 1996.
 */
const char*
inet_ntop (int af, const void */*restrict*/ src
	, char */*restrict*/ dst, socklen_t cnt)
{
	switch (af)
	{
	case AF_INET:
		return (inet_ntop4 (src, dst, cnt));

	case AF_INET6:
		return (inet_ntop6 (src, dst, cnt));

	default:
		return (NULL);
	}
	/* NOTREACHED */
}

static inline int decode_tcp(struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
	skb_set_transport_header(skb, 0);
#else
	SKB_TCP_HEAD_ADDR(skb) = (struct tcphdr *)skb->data;
#endif
	return 0;
}

static inline int decode_udp(struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
	skb_set_transport_header(skb, 0);
#else
	SKB_L4_HEAD_ADDR(skb) = skb->data;
#endif
	return 0;
}

static inline int decode_icmp(struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
	skb_set_transport_header(skb, 0);
#else
	SKB_L4_HEAD_ADDR(skb) = skb->data;
#endif
	return 0;
}

static inline int decode_icmpv6(struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
	skb_set_transport_header(skb, 0);
#else
	SKB_L4_HEAD_ADDR(skb) = skb->data;
#endif
	return 0;
}

static inline int decode_ip_proto(struct sk_buff *skb, uint8_t ip_proto)
{
	int ret = -1;

	switch (ip_proto)
	{
		case IPPROTO_TCP:
			ret = decode_tcp(skb);
			break;
		case IPPROTO_UDP:
			ret = decode_udp(skb);
			break;
		case IPPROTO_ICMP:
			ret = decode_icmp(skb);
			break;
		case IPPROTO_ICMPV6:
			ret = decode_icmpv6(skb);
			break;
		default:
			DBG("Unknown IP proto %u", SKB_IP_PRO(skb));
			break;
	}

	return ret;
}

typedef struct
{
    unsigned char nexthdr;
    unsigned char hdrlen;

    /* option dependent data */
    unsigned char opts[0];
} ip6eh_general_t;

static inline int decode_ipv6(struct sk_buff *skb)
{
	int ret = 0;
	uint8_t nexthdr = 0;
	uint32_t hdr_len = 0;
	ip6eh_general_t *ip6eh_gen = NULL;

	if (skb->len < sizeof(struct ipv6hdr))
	{
		DBG("Invalid input skb->len=%u", skb->len);
		return -1;
	}

	nexthdr = SKB_IPV6_NHDR(skb);

	skb_pull(skb, sizeof(struct ipv6hdr));

	ip6eh_gen = (ip6eh_general_t *)skb->data;

	while (1)
	{
		DBG("nexthdr=%u\n", nexthdr);

		switch (nexthdr)
		{
			case IP6_IPPROTO_HOPOPTS:
			case IP6_IPPROTO_IP6_ROUTING:
			case IP6_IPPROTO_IP6_DSTOPTS:
			case IP6_IPPROTO_MH:
				break;

			case IP6_IPPROTO_AH:
			case IP6_IPPROTO_ESP:
			case IP6_IPPROTO_IP6_FRAGMENT:
				goto EXIT; // Do not support

			default:
				/*
				 * For any other proto, it'd be in layer 4 or above.
				 */
				DBG("End ip6 decode at ip6 nexthdr = %u\n", nexthdr);
				goto END_DECODE_IP6EH;
		}

		/* Get next header */
		hdr_len += (8 + (ip6eh_gen->hdrlen * 8));

		if (skb->len >= hdr_len)
		{
			ip6eh_gen = (ip6eh_general_t *)(skb->data + hdr_len);
			nexthdr = ip6eh_gen->nexthdr;
		}
		else
		{
			goto EXIT;
		}
	}

END_DECODE_IP6EH:

	if (skb->len >= hdr_len)
	{
		skb_pull(skb, hdr_len);
		ret = decode_ip_proto(skb, nexthdr);
		skb_push(skb, hdr_len);
	}

EXIT:
	skb_push(skb, sizeof(struct ipv6hdr));

	return ret;
}

static inline int decode_ipv4(struct sk_buff *skb)
{
	unsigned int ip_head_len = 0;
	unsigned char flags = 0;
	unsigned int fragment = 0;
	int ret = -1;

	if (unlikely(skb->len < 20 || SKB_IP_IHL(skb) < 5))
	{
		DBG("skb->len %d have some error\n", skb->len);
		return ret;
	}
	if (unlikely(skb->len < (SKB_IP_IHL(skb) << 2)))
	{
		DBG("skb->len %d < ip header\n", skb->len);
		return ret;
	}

	ip_head_len = (unsigned int)(SKB_IP_IHL(skb) << 2);
	flags = (unsigned char)((ntohs(SKB_IP_FRAG_OFF(skb)) & 0xe000) >>  13);
	fragment = (unsigned int)(ntohs(SKB_IP_FRAG_OFF(skb)) & IP_OFFMASK);

	/* fragment */
	if ((flags & IP_FLAG_MF) || fragment)
	{
		DBG("IP fragment\n");
		return ret;
	}

	skb_pull(skb, ip_head_len);
	ret = decode_ip_proto(skb, SKB_IP_PRO(skb));
	skb_push(skb, ip_head_len);
	
	return ret;
}

extern char *dev_wan;
extern unsigned int mode;

bool is_skb_upload(struct sk_buff *skb, const struct net_device *fw_outdev)
{
	if (skb->dev)
	{
		udb_shell_wan_detection(skb->dev->name, skb->len);
	}

	if (mode == 0) /*gw mode*/
	{
		if (unlikely(!skb->dev))
		{
			if (fw_outdev)
			{
				return (strstr(dev_wan, fw_outdev->name)) ? true : false;
			}
			DBG("skb->dev is NULL\n");
			return 0;
		}

		return (NULL == strstr(dev_wan, skb->dev->name)) ? true : false;
	}
	else if (mode == 1) /*ap mode*/
	{
		if (unlikely(!skb->dev || !strcmp(skb->dev->name, "lo")))
		{
			DBG("skb->dev is NULL\n");
			return 0;
		}

		return (NULL == strstr(dev_wan, skb->dev->name)) ? false : true;
	}
#ifdef CONFIG_BRIDGE_NETFILTER
	else if (mode == 2) /*br mode, case [Single interface br0]*/
	{
		if (unlikely(!skb->nf_bridge)) /*W -> L */
		{
			//printk("WARNING: skb->phydev name  is NULL\n");
			return 0;
		}
		else
		{
			char *indev, *outdev;
			indev = skb->nf_bridge->physindev ? skb->nf_bridge->physindev->name : "null";
			outdev = skb->nf_bridge->physoutdev ? skb->nf_bridge->physoutdev->name : "null";
			//printk("Phyicsial dev-name: in-%s / out-%s.\n",indev, outdev);
			return (NULL == strstr(dev_wan, indev)) ? true : false;
		}
	}
	else
#endif
	if (mode == 3) /*new gw mode*/
	{
		struct dst_entry *dst = NULL;
		dst = skb_dst(skb);
		if (unlikely(!dst || !dst->dev || !strcmp(dst->dev->name, "lo")))
		{
			DBG("dst->dev is NULL\n");
			return 0;
		}

		return (NULL == strstr(dev_wan, dst->dev->name)) ? false : true;
	}
	else
	{
		printk("mode %u not support\n", mode);
		return false;
	}
}

int parse_skb(struct sk_buff *skb)
{
	int ret = -1;

	if (skb_is_nonlinear(skb)) 
	{
		DBG("skb is non linear\n");
		if (0 != skb_linearize(skb))
		{
			DBG("linearize skb failed\n");
			return ret;
		}
	}

	if (!SKB_ETH_ADDR(skb))
	{
		/*
		 * NOTE LOCAL_OUT packets have L2 header unassigned yet;
		 * besides, both L3 & L4 header ptrs are valid so we don't
		 * have to decode them.
		 */
		ret = 0;
	}
	else
	{
		switch (ntohs(skb->protocol))
		{
			case ETH_P_IP:
				ret = decode_ipv4(skb);
				break;
			case ETH_P_IPV6:
				ret = decode_ipv6(skb);
				break;
			default:
				break;
		}
	}

	return ret;
}

/* Compute ip checksum */
unsigned short ip_sum(unsigned short *buf, int len)
{
	unsigned int sum = 0; /* assumes long == 32 bits */
	unsigned short oddbyte = 0;
	unsigned short answer = 0; /* assumes USHORT == 16 bits */

	while (len > 1)
	{
		sum += *buf++;
		len -= 2;
	}

	if (1 == len)
	{
		*((unsigned char *) &oddbyte) = *(unsigned char *) buf; /* one byte only */
		sum += oddbyte;
	}

	sum = (sum >> 16) + (sum & 0xffff); /* add high-16 to low-16 */
	sum += (sum >> 16); /* add carry */
	answer = ~sum; /* ones-complement, then truncate to 16 bits */
	DBG("answer(%x)\n",answer);
	return (answer);
}

#define MAC_OCTET_FMT "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_OCTET_EXPAND(o) \
	(uint8_t) o[0], (uint8_t) o[1], (uint8_t) o[2], (uint8_t) o[3], (uint8_t) o[4], (uint8_t) o[5]

#define GET_DIP_BY_SKB(ip, ip_ver, skb) \
	ip_ver = (*(unsigned char *)SKB_L3_HEAD_ADDR(skb)) >> 4;\
	ip = (4 == ip_ver) ?\
		(unsigned char *)&(SKB_IP_DIP(skb)) : SKB_IPV6_DIP(skb);

#define GET_SIP_BY_SKB(ip, ip_ver, skb) \
	ip_ver = (*(unsigned char *)SKB_L3_HEAD_ADDR(skb)) >> 4;\
	ip = (4 == ip_ver) ?\
		(unsigned char *)&(SKB_IP_SIP(skb)) : SKB_IPV6_SIP(skb);

__attribute__((unused))
void dump_skb(struct sk_buff *skb)
{
	char *proto = "";
	char unknown[8];

	char *sip = NULL, *dip = NULL;
	char sip_str[INET6_ADDRSTRLEN], dip_str[INET6_ADDRSTRLEN];

	char ip_ver = 0;
	char ip_pro = 0;
	int af = 0;

	unsigned short sport = 0;
	unsigned short dport = 0;

	if (unlikely(!SKB_ETH(skb)))
	{
		return;
	}

	ip_ver = (__SKB_ETH_PRO(skb) == htons(ETH_P_IPV6)) ? 6 : 4;
	ip_pro = (6 == ip_ver) ? SKB_IPV6_NHDR(skb) : SKB_IP_PRO(skb);

	switch (ip_pro)
	{
		case IPPROTO_TCP:
			proto = "TCP";
			break;
		case IPPROTO_UDP:
			proto = "UDP";
			break;
		case IPPROTO_ICMP:
			proto = "ICMP";
			break;
		case IPPROTO_IGMP:
			proto = "IGMP";
			break;
		case IPPROTO_IPV6:
			proto = "IPv6";
			break;
		case IPPROTO_ICMPV6:
			proto = "IPv6";
			break;
		default:
			proto = unknown;
			snprintf(unknown, sizeof(unknown), "%d", SKB_IP_PRO(skb));
			break;
	}

	/* convert IP octecs to strings */
	af = (4 == ip_ver) ? AF_INET : AF_INET6;
	sip = (4 == ip_ver) ? (char*)&SKB_IP_SIP(skb) : (char*)&SKB_IPV6_IN6SIP(skb);
	dip = (4 == ip_ver) ? (char*)&SKB_IP_DIP(skb) : (char*)&SKB_IPV6_IN6DIP(skb);
	inet_ntop(af, sip, sip_str, sizeof(sip_str));
	inet_ntop(af, dip, dip_str, sizeof(dip_str));

	if (IPPROTO_TCP == ip_pro)
	{
		skb_pull(skb, SKB_IP_IHL(skb) * 4);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
		skb_set_transport_header(skb, 0);
#else
		SKB_TCP_HEAD_ADDR(skb) = (struct tcphdr *)skb->data;
#endif
		skb_push(skb, SKB_IP_IHL(skb) * 4);
		sport = ntohs(SKB_TCP_SPORT(skb));
		dport = ntohs(SKB_TCP_DPORT(skb));
	}
	else if (IPPROTO_UDP == ip_pro)
	{
		skb_pull(skb, SKB_IP_IHL(skb) * 4);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
		skb_set_transport_header(skb, 0);
#else
		SKB_L4_HEAD_ADDR(skb) = skb->data;
#endif
		skb_push(skb, SKB_IP_IHL(skb) * 4);
		sport = ntohs(SKB_UDP_SPORT(skb));
		dport = ntohs(SKB_UDP_DPORT(skb));
	}

	printk("* %s, %s:%u -> %s:%u (pkt=%d, hlen=%d) mark=0x%x '"
		"smac=" MAC_OCTET_FMT " (%s) dmac=" MAC_OCTET_FMT "\n",
		proto,
		sip_str, sport,
		dip_str, dport,
		ntohs(SKB_IP_TOT_LEN(skb)),
		SKB_IP_IHL(skb) * 4,
		skb->mark,
		MAC_OCTET_EXPAND(SKB_ETH_SRC(skb)),
		skb->dev->name,
		MAC_OCTET_EXPAND(SKB_ETH_DST(skb)));
}

