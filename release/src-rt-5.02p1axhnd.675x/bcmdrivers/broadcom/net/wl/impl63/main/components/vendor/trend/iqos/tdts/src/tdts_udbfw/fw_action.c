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

#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/if_ether.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>


#include "forward_config.h"
#include "fw_util.h"
#include "skb_access.h"
#include "fw_action.h"
#include "fw_internal.h"

static unsigned char redirect_head[] =
	"HTTP/1.1 200 OK\r\nServer: Jetty/4.2.x (Windows XP/5.1 x86 java/1.6.0_17)\r\nContent-Type: text/html\r\nContent-Length: ";
static unsigned char redirect_head2[] = "\r\nAccept-Ranges: bytes\r\n\r\n";
static unsigned char redirect_head3[] = "<html>\r\n<head>\r\n<meta HTTP-EQUIV=\"REFRESH\" content=\"0; url=";
static unsigned char redirect_tail[] = "\">\r\n</head>\r\n<body></body>\r\n</html>\n\r\n";

/* The direction of skb is different */
void send_action_pkt(struct sk_buff *skb
		, struct sock *sk
		, void *send
		, char *wan_name
		, char *redir_url, int url_len
		, struct net_device *in_dev
		, struct net_device *out_dev)
{
	struct sk_buff *act_skb = NULL;
	unsigned short l2_len = 0, len = 0, data_len = 0;
	unsigned int act_ack = 0;
	pseudohdr_t2 *pseudohdr = NULL;
	pseudohdr_v6_t2 *pseudohdr_v6 = NULL;
//	bool is_vlan_pkt = false;
	unsigned char ip_ver = 0;
	unsigned char cont_len_str[4];
	int offset = 0;

	struct net_device *wan_dev = NULL;
	unsigned int addr_type = RTN_UNSPEC;
	bool is_upload = false;
	uint32_t iphl = 0;
	bool nexth = false;
	int ret = 0;

	struct nf_conn *conntrack = NULL;
	enum ip_conntrack_info ctinfo;
	
	if (NULL == skb)
	{
		return;
	}

	is_upload = is_skb_upload(skb, out_dev);
	conntrack = nf_ct_get(skb, &ctinfo);

	/* sizeof(redirect_head) will contain '\0' */
	DBG("sizeof(redirect_head) = %d, sizeof(redirect_tail) = %u\n"
		, sizeof(redirect_head), sizeof(redirect_tail));

	ip_ver = ((ETH_P_IP == ntohs(skb->protocol)) ? 4 : 6);

	assert(SKB_IP_HEAD_ADDR(skb));

	l2_len = ETH_HLEN + ETH_FCS_LEN;

#if 0
	if (IS_PKT_VLAN(pkt))
	{
		if (NULL != pkt->isl_hdr)
		{
			ret = -1;
			goto __done;
		}

		if (NULL != pkt->v_hdr)
		{
			is_vlan_pkt = true;
			l2_len += sizeof(struct nk_vlan_hdr);
		}
	}
#endif

	/*
	 * TCP header
	 * + IP header
	 * + Ethernet header & trailer
	 * + reserved for alignment
	 */
	/* delete 4 '\0' and add 3 byte (content length) */
	if (NULL != redir_url)
	{
		data_len = sizeof(redirect_head)
				+ sizeof(redirect_head2)
				+ sizeof(redirect_head3)
				+ url_len + sizeof(redirect_tail) - 4 + 3;
		sprintf(cont_len_str, "%d", (int)(sizeof(redirect_head3) + url_len + sizeof(redirect_tail) - 2));
	}

	len = TCP_HLEN + ((4 == ip_ver) ? IP_HLEN : IP6_HLEN) + l2_len + 2 + data_len;

	if (!(act_skb = alloc_skb(len, GFP_ATOMIC)))
	{
		ret = -3;
		goto __done;
	}

	skb_reserve(act_skb, 2);

	/* Ethernet header */
	skb_set_mac_header(act_skb, (skb_tail_pointer(act_skb) - act_skb->data));
	/* reverse the MACs */

	if(!SKB_ETH(skb))
	{
		ret = -7;
		ERR("skb has no ethernet header!\n");
		goto __done;
	}

	if(!SKB_ETH(act_skb))
	{
		ret = -7;
		ERR("act_skb has no ethernet header!\n");
		goto __done;
	}

	if (is_upload)
	{
		memcpy(__SKB_ETH_DST(act_skb), __SKB_ETH_SRC(skb), ETH_ALEN);
		memcpy(__SKB_ETH_SRC(act_skb), __SKB_ETH_DST(skb), ETH_ALEN);
	}
	else
	{
		memcpy(__SKB_ETH_DST(act_skb), __SKB_ETH_DST(skb), ETH_ALEN);
		memcpy(__SKB_ETH_SRC(act_skb), __SKB_ETH_SRC(skb), ETH_ALEN);
	}
	SKB_ETH(act_skb)->h_proto = (4 == ip_ver) ? htons(ETH_P_IP) : htons(ETH_P_IPV6);
	skb_put(act_skb, ETH_HLEN);

#if 0
	/* 802.1Q VLAN header */
	if (is_vlan_pkt)
	{
		struct nk_vlan_hdr *v_hdr = NULL;

		SKB_ETH_PRO(act_skb) = htons(ETHERNET_TYPE_8021Q);
		memcpy(skb_put (act_skb, sizeof(struct nk_vlan_hdr))
			, pkt->v_hdr, sizeof(struct nk_vlan_hdr));
		v_hdr = (struct nk_vlan_hdr *)(act_skb->data + ETH_HLEN);

		v_hdr->h_vlan_encapsulated_proto =
			(4 == ip_ver) ? htons(ETH_P_IP) : htons(ETH_P_IPV6);
	}
#endif

	/* IP header */
	if (4 == ip_ver)
	{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
		skb_set_network_header(act_skb, (skb_tail_pointer(act_skb) - act_skb->data));
#else
		SKB_IP(act_skb) = (struct iphdr *)act_skb->tail;
#endif
		iphl = (uint32_t) (SKB_IP_IHL(skb) << 2);
		SKB_IP_VER(act_skb) = 4;
		SKB_IP_IHL(act_skb) = IP_HLEN >> 2;
		SKB_IP_TOT_LEN(act_skb) = htons(IP_HLEN + TCP_HLEN + data_len);
		SKB_IP_ID(act_skb) = SKB_IP_ID(skb);
		if (is_upload)
		{
			if (conntrack)
			{
		
				SKB_IP_SIP(act_skb) = conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip;
				SKB_IP_DIP(act_skb) = conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip;
			}
			else
			{
				SKB_IP_SIP(act_skb) = SKB_IP_DIP(skb);
				SKB_IP_DIP(act_skb) = SKB_IP_SIP(skb);
			}
		}
		else
		{
			if (conntrack)
			{
				
				SKB_IP_SIP(act_skb) = conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip;
				SKB_IP_DIP(act_skb) = conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip;
			}
			else
			{
				SKB_IP_SIP(act_skb) = SKB_IP_SIP(skb);
				SKB_IP_DIP(act_skb) = SKB_IP_DIP(skb);
			}
		}
		SKB_IP_FRAG_OFF(act_skb) = 0;
		SKB_IP_TTL(act_skb) = 255;
		SKB_IP_PRO(act_skb) = IPPROTO_TCP;
		SKB_IP_CHECK(act_skb) = 0;
		SKB_IP_CHECK(act_skb) =
			ip_sum((unsigned short *)SKB_L3_HEAD_ADDR(act_skb), IP_HLEN);

		skb_put(act_skb, IP_HLEN);
		skb_pull(skb, iphl); 
	}
	else
	{
		uint8_t nexthdr;
		skb_set_network_header(act_skb, (skb_tail_pointer(act_skb) - act_skb->data));

		nexthdr = SKB_IPV6_NHDR(skb);
		skb_pull(skb, sizeof(struct ipv6hdr));

		if (nexthdr == IPPROTO_HOPOPTS && skb->len >= sizeof(ipv6_opt_hdr_t))
		{
			ipv6_opt_hdr_t *ohdr = (ipv6_opt_hdr_t *) skb->data;
			nexthdr = ohdr->nexthdr;
			skb_pull(skb, sizeof(ipv6_opt_hdr_t));
			nexth = true;
		}

		SKB_IPV6_VER(act_skb) = 6;
		SKB_IPV6_PRIO(act_skb) = 0;
		memset(SKB_IPV6_FLB(act_skb), 0, sizeof(__u8) * 3);
		SKB_IPV6_PLEN(act_skb) = htons(TCP_HLEN + data_len);		// Not contain IP header length
		SKB_IPV6_HOPL(act_skb) = 255;
		SKB_IPV6_NHDR(act_skb) = IPPROTO_TCP;
		if (is_upload)
		{
			IP_COPY(SKB_IPV6_SIP(act_skb), SKB_IPV6_DIP(skb), ip_ver);
			IP_COPY(SKB_IPV6_DIP(act_skb), SKB_IPV6_SIP(skb), ip_ver);
		}
		else
		{
			IP_COPY(SKB_IPV6_SIP(act_skb), SKB_IPV6_SIP(skb), ip_ver);
			IP_COPY(SKB_IPV6_DIP(act_skb), SKB_IPV6_DIP(skb), ip_ver);
		}

		skb_put(act_skb, IP6_HLEN);
	}

	/* TCP header */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	skb_set_transport_header(act_skb, (skb_tail_pointer(act_skb) - act_skb->data));
	skb_set_transport_header(skb, 0);
#else
	SKB_TCP(act_skb) = (struct tcphdr *)act_skb->tail;
	SKB_TCP_HEAD_ADDR(skb) = (struct tcphdr *) skb->data;
#endif

	SKB_TCP_HLEN(act_skb) = TCP_HLEN >> 2;
	if (is_upload)
	{
		SKB_TCP_SPORT(act_skb) = SKB_TCP_DPORT(skb);
		SKB_TCP_DPORT(act_skb) = SKB_TCP_SPORT(skb);

		if (4 == ip_ver)
		{
			DBG("seq = 0x%x, ttl = 0x%x, ipl = 0x%x, tcp_hdl = 0x%x\n"
				, ntohl(SKB_TCP_SEQ(skb)), ntohs(SKB_IP_TOT_LEN(skb))
				, SKB_IP_IHL(skb), SKB_TCP_HLEN(skb));

			act_ack = ntohl(SKB_TCP_SEQ(skb)) + (ntohs(SKB_IP_TOT_LEN(skb))
				- (SKB_IP_IHL(skb) << 2) - (SKB_TCP_HLEN(skb) << 2));
		}
		else
		{
			act_ack = ntohl(SKB_TCP_SEQ(skb)) + (ntohs(SKB_IPV6_PLEN(skb))
				- (SKB_TCP_HLEN(skb) << 2));
		}

		SKB_TCP_SEQ(act_skb) = SKB_TCP_ACK(skb);
		SKB_TCP_ACK(act_skb) = htonl(act_ack);
	}
	else
	{
		SKB_TCP_SPORT(act_skb) = SKB_TCP_SPORT(skb);
		SKB_TCP_DPORT(act_skb) = SKB_TCP_DPORT(skb);

		SKB_TCP_SEQ(act_skb) = SKB_TCP_SEQ(skb);
		SKB_TCP_ACK(act_skb) = SKB_TCP_ACK(skb);
	}

	SKB_TCP_FLAGS_SYN(act_skb) = 0;

	if (NULL != redir_url)
	{
		SKB_TCP_FLAGS_RST(act_skb) = 0;
		SKB_TCP_FLAGS_ACK(act_skb) = 1;
		SKB_TCP_FLAGS_FIN(act_skb) = 1;
		SKB_TCP_FLAGS_PSH(act_skb) = 1;
	}
	else
	{
		SKB_TCP_FLAGS_RST(act_skb) = 1;
		SKB_TCP_FLAGS_ACK(act_skb) = 1;
		SKB_TCP_FLAGS_FIN(act_skb) = 0;
		SKB_TCP_FLAGS_PSH(act_skb) = 0;
	}
	SKB_TCP_FLAGS_URG(act_skb) = 0;
	SKB_TCP_FLAGS_ECE(act_skb) = 0;
	SKB_TCP_FLAGS_CWR(act_skb) = 0;
	SKB_TCP_WIN(act_skb) = htons(1 << 14);
	SKB_TCP_URG(act_skb) = 0;
	SKB_TCP_CHECK(act_skb) = 0;

	skb_put(act_skb, TCP_HLEN);

	if (NULL != redir_url)
	{
		/* put the message */
		memcpy(skb_tail_pointer(act_skb), redirect_head, sizeof(redirect_head) - 1);
		offset += sizeof(redirect_head) - 1;

		memcpy(skb_tail_pointer(act_skb) + offset, cont_len_str, 3);
		offset += 3;
	
		memcpy(skb_tail_pointer(act_skb) + offset, redirect_head2, sizeof(redirect_head2) - 1);
		offset += sizeof(redirect_head2) - 1;

		memcpy(skb_tail_pointer(act_skb) + offset, redirect_head3, sizeof(redirect_head3) - 1);
		offset += sizeof(redirect_head3) - 1;

		memcpy(skb_tail_pointer(act_skb) + offset, redir_url, url_len);
		offset += url_len;

		memcpy(skb_tail_pointer(act_skb) + offset, redirect_tail, sizeof(redirect_tail) - 1);
		offset += sizeof(redirect_tail) - 1;

		skb_put(act_skb, data_len);
	}

	/* Compute TCP checksum */
	if (4 == ip_ver)
	{
		pseudohdr = KMALLOC_INIT(sizeof(pseudohdr_t2), GFP_ATOMIC);
		if (unlikely(NULL == pseudohdr))
		{
			ret = -4;
			goto __done;
		}

		pseudohdr->sip = SKB_IP_SIP(act_skb);
		pseudohdr->dip = SKB_IP_DIP(act_skb);
		pseudohdr->zero = 0;
		pseudohdr->ip_pro = IPPROTO_TCP;
		pseudohdr->len = htons(20 + data_len);
		memcpy((char *)(pseudohdr->data), SKB_TCP_HEAD_ADDR(act_skb), 20 + data_len);
		/* 32 mean pesudo header(12) + TCP header(20) */
		SKB_TCP_CHECK(act_skb) = ip_sum((unsigned short *)pseudohdr, 32 + data_len);

		KFREE_INIT(pseudohdr, sizeof(pseudohdr_t2));
		pseudohdr = NULL;

		skb_push(skb, iphl);
	}
	else
	{
		pseudohdr_v6 = KMALLOC_INIT(sizeof(pseudohdr_v6_t2), GFP_ATOMIC);
		if (unlikely(NULL == pseudohdr_v6))
		{
			ret = -4;
			goto __done;
		}

		IP_COPY(pseudohdr_v6->sip, SKB_IPV6_SIP(act_skb), ip_ver);
		IP_COPY(pseudohdr_v6->dip, SKB_IPV6_DIP(act_skb), ip_ver);
		pseudohdr_v6->len = htonl(20 + data_len);
		memset(pseudohdr_v6->zero, 0, sizeof(unsigned char) * 3);
		pseudohdr_v6->next_hdr = IPPROTO_TCP;

		memcpy((char *)(pseudohdr_v6->data), SKB_TCP_HEAD_ADDR(act_skb), 20 + data_len);
		SKB_TCP_CHECK(act_skb) = ip_sum((unsigned short *)pseudohdr_v6, 60 + data_len);

		KFREE_INIT(pseudohdr_v6, sizeof(pseudohdr_v6_t2));
		pseudohdr_v6 = NULL;

		if (nexth)
		{
			skb_push(skb, sizeof(ipv6_opt_hdr_t));
		}
		skb_push(skb, sizeof(struct ipv6hdr));
	}

	skb_pull(act_skb, ETH_HLEN);
	act_skb->len = ((4 == ip_ver) ? IP_HLEN : IP6_HLEN) + TCP_HLEN + data_len;


#ifdef CONFIG_BRIDGE_NETFILTER
	if (skb->nf_bridge)
	{
		act_skb->nf_bridge = KMALLOC_INIT(sizeof(struct nf_bridge_info), GFP_ATOMIC);
		if (likely(act_skb->nf_bridge))
		{
			memcpy(act_skb->nf_bridge, skb->nf_bridge, sizeof(struct nf_bridge_info));
			atomic_set(&(act_skb->nf_bridge->use), 1);
		}
	}
#endif
#if 0
	act_skb->skb_iif = skb->skb_iif;
	act_skb->sk = skb->sk;
	act_skb->tstamp = skb->tstamp;
	act_skb->ip_summed = CHECKSUM_PARTIAL;
	act_skb->csum_start = (unsigned char *)SKB_TCP_HEAD_ADDR(act_skb) - act_skb->head;
	act_skb->csum_offset = (unsigned char *)&SKB_TCP_CHECK(act_skb) - (unsigned char *)SKB_TCP_HEAD_ADDR(act_skb);

  #ifdef CONFIG_BRIDGE_NETFILTER
	act_skb->nf_bridge = skb->nf_bridge;
  #endif
	act_skb->queue_mapping = skb->queue_mapping;
	#ifdef CONFIG_IPV6_NDISC_NODETYPE
	act_skb->ndisc_nodetype = skb->ndisc_nodetype;
	#endif
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	act_skb->vlan_tci = skb->vlan_tci;
#endif
	if (is_upload)
	{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	if (NULL == (wan_dev = dev_get_by_name(&init_net, out_dev->name)))
#else
	if (NULL == (wan_dev = dev_get_by_name(out_dev->name)))
#endif
		{
			ret = -5;
			goto __done;
		}

		act_skb->dev = wan_dev;

		rcu_read_lock();
		dev_put(wan_dev);
		rcu_read_unlock();
	}
	else
	{
		act_skb->dev = skb->dev;
	}

	atomic_set(&act_skb->users, 1);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	skb_dst_set_noref(act_skb, skb_dst(skb));
#endif
	act_skb->protocol = htons(ETH_P_IP);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0))
        if ((4 == ip_ver) ? ip_route_me_harder(&init_net, act_skb, addr_type) : ip6_route_me_harder(&init_net, act_skb))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if ((4 == ip_ver) ? ip_route_me_harder(act_skb, addr_type) : ip6_route_me_harder(act_skb))
#else
	if (ip_route_me_harder(&act_skb, addr_type))
#endif
	{
		ret = -6;
		goto __done;
	}

	nf_ct_attach(act_skb, skb);

//	act_skb->nfctinfo = skb->nfctinfo;
	act_skb->nfctinfo = 3;

	DBG("sk: %p\t%p\n", act_skb->sk, skb->sk);
	DBG("tstamp: %d\t%d\n", act_skb->tstamp, skb->tstamp);
	DBG("dev: %p\t%p\n", act_skb->dev, skb->dev);
	DBG("nfctinfo: %u\t%u\n", act_skb->nfctinfo, skb->nfctinfo);
#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
	DBG("nfct: %p\t%p\n", act_skb->nfct, skb->nfct);
#endif
#ifdef CONFIG_BRIDGE_NETFILTER
	DBG("nf_bridge: %p\t%p\n", act_skb->nf_bridge, skb->nf_bridge);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	DBG("_skb_refdst: %lu\t%lu\n", act_skb->_skb_refdst, skb->_skb_refdst);
	DBG("skb_iif: %d\t%d\n", act_skb->skb_iif, skb->skb_iif);
	DBG("queue_mapping: %d\t%d\n", act_skb->queue_mapping, skb->queue_mapping);
	DBG("vlan_tci: %u\t%u\n", act_skb->vlan_tci, skb->vlan_tci);
#ifdef CONFIG_IPV6_NDISC_NODETYPE
	DBG("ndisc_nodetype: %d\t%d\n", act_skb->ndisc_nodetype, skb->ndisc_nodetype);
#endif
#endif
	
__done:
	if (ret < 0)
	{
		if (act_skb)
		{
			kfree_skb(act_skb);
		}

#ifdef CONFIG_BRIDGE_NETFILTER
		if (act_skb->nf_bridge)
		{
			KFREE_INIT(act_skb->nf_bridge, sizeof(struct nf_bridge_info));
		}
#endif
	}
	else
	{
		DBG("send act_skb\n");
		//send(act_skb);

		MY_NF_HOOK_THRESH(
			(4 == ip_ver) ? PF_INET : PF_INET6,
			NF_INET_FORWARD,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
			&init_net,
#endif 
			sk,
			act_skb, 
			in_dev, out_dev, 
			send, 
			(4 == ip_ver) ? (NF_IP_PRI_FILTER + 1) : (NF_IP6_PRI_FILTER + 1));
	}
}

extern char *dev_wan;

int send_redir_page(void *skb_ptr, redir_param_t *redir_param, tdts_net_device_t *dev)
{
	char *redir_url = NULL;
	int url_len = 256, len = 0;
	struct sk_buff *skb = (struct sk_buff *)skb_ptr;
	int pos = 0;

	if (!skb_ptr || !dev || !redir_param)
	{
		return -1;
	}

	if (!(redir_url = kmalloc(url_len, GFP_ATOMIC)))
	{
		ERR("failed to allocate %d bytes of memory\n", url_len);
		return -1;
	}

	if (redir_param->redir_base)
	{
		len += snprintf(redir_url + len, url_len - len, "%s?", redir_param->redir_base);
	}

	if (redir_param->cat_id != 0)
	{
		len += snprintf(redir_url + len, url_len - len, "cat_id=%d", redir_param->cat_id);
	}

	if (redir_param->app_id > 0)
	{
		len += snprintf(redir_url + len, url_len - len, "app_cid=%u&app_id=%u", redir_param->app_cid, redir_param->app_id);
	}

	if (redir_param->wbl >= 0)
	{
		len += snprintf(redir_url + len, url_len - len, "&wbl=%u", redir_param->wbl);
	}

	if (redir_param->gid > 0)
	{
		len += snprintf(redir_url + len, url_len - len, "&gid=%u&pid=%u", redir_param->gid, redir_param->pid);
	}
	
	if (redir_param->mac)
	{
		len += snprintf(redir_url + len, url_len - len, "&mac=%02X%02X%02X%02X%02X%02X",
			redir_param->mac[0], redir_param->mac[1], redir_param->mac[2], 
			redir_param->mac[3], redir_param->mac[4], redir_param->mac[5]);
	}
	
	if (redir_param->orig_domain_len && redir_param->orig_path)
	{
		// dismiss URL query string
		for (pos = 0; pos < redir_param->orig_path_len; pos++)
		{
			if ('?' == redir_param->orig_path[pos])
			{
				break;
			}
		}
		
		len += snprintf(redir_url + len, url_len - len, "&domain=%.*s%.*s", 
			redir_param->orig_domain_len, redir_param->orig_domain, pos, redir_param->orig_path);
	}

	if (len > 0)
	{
		//printk("# redir_url=%s\n", redir_url);
	}

	send_action_pkt(
		skb,
		dev->fw_sk,
		dev->fw_send,
		dev_wan,
		redir_param->redir_base ? redir_url : NULL,
		redir_param->redir_base ? len : 0,
		dev->fw_indev,
		dev->fw_outdev);

	if (redir_url)
	{
		kfree(redir_url);
	}

	return 0;
}
