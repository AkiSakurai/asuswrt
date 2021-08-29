/*
 *  ebt_arpnat
 *
 *	Authors:
 *      Kestutis Barkauskas <gpl@wilibox.com>
 *
 *  November, 2005
 *
 *	Rewritten by:
 *         Kestutis Barkauskas and Kestutis Kupciunas <gpl@ubnt.com>
 *
 *  June, 2010
 *
 *      Updated to work with more recent kernel versions (e.g., 2.6.30)
 *      Ditched entry expiration in favor of wiping entries with duplicate ips, when situation arises
 *      Fixed arpnat procfs (though both arpnat_cache and arpnat_info are both in root procfs directory now)
 *
 *      Eric Bishop <eric@gargoyle-router.com>
 */




#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_nat.h>
#include <linux/module.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_pppox.h>
#include <linux/if_vlan.h>
#include <linux/rtnetlink.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/inetdevice.h>
#include <net/arp.h>
#include <net/ip.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <net/checksum.h>


#include "../br_private.h"

#define STRMAC "%02x:%02x:%02x:%02x:%02x:%02x"
#define STRIP "%d.%d.%d.%d"
#define MAC2STR(x) (x)[0],(x)[1],(x)[2],(x)[3],(x)[4],(x)[5]
#define IP2STR(x) (x)>>24&0xff,(x)>>16&0xff,(x)>>8&0xff,(x)&0xff

#define GIADDR_OFFSET (24)



//#define ARPNAT_DEBUG 1


#ifdef ARPNAT_DEBUG
static uint8_t debug = 1;
#else
static uint8_t debug = 0;
#endif



#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

struct arpnat_dat
{
	uint32_t ip;
	uint8_t mac[ETH_ALEN];
} __packed;

struct mac2ip
{
	struct hlist_node node;
	struct arpnat_dat data;
};

static HLIST_HEAD(arpnat_table);
static spinlock_t arpnat_lock = __SPIN_LOCK_UNLOCKED(arpnat_lock);

static uint8_t bootpnat = 1;

static struct mac2ip* find_mac_nat(struct hlist_head* head, const uint8_t* mac)
{
	struct mac2ip* tpos;
	struct mac2ip* result = NULL;
	struct hlist_node* pos;
	struct hlist_node* n;
	hlist_for_each_entry_safe(tpos, pos, n, head, node)
	{
		if (memcmp(tpos->data.mac, mac, ETH_ALEN) == 0)
		{
			result = tpos;
			break;
		}
	}
	return result;
}

static struct mac2ip* find_ip_nat(struct hlist_head* head, uint32_t ip)
{
	struct mac2ip* tpos;
	struct mac2ip* result = NULL;
	struct hlist_node* pos;
	struct hlist_node* n;

	hlist_for_each_entry_safe(tpos, pos, n, head, node)
	{
		if (tpos->data.ip == ip)
		{
			result = tpos;
			break;
		}
	}
	return result;
}


static void clear_ip_nat(struct hlist_head* head, uint32_t ip)
{
	struct mac2ip* tpos;
	struct hlist_node* pos;
	struct hlist_node* n;

	hlist_for_each_entry_safe(tpos, pos, n, head, node)
	{
		if (tpos->data.ip == ip)
		{
			hlist_del(pos);
			kfree(tpos);
		}
	}
}

static void free_arp_nat(struct hlist_head* head)
{
	struct mac2ip* tpos;
	struct hlist_node* pos;
	struct hlist_node* n;
	hlist_for_each_entry_safe(tpos, pos, n, head, node)
	{
		hlist_del(pos);
		kfree(tpos);
	}
}

static struct mac2ip* update_arp_nat(struct hlist_head* head, const uint8_t* mac, uint32_t ip)
{
	struct mac2ip* entry;

	entry = find_mac_nat(head, mac);
	if (!entry)
	{
		clear_ip_nat(head, ip); /* if entries with new ip exist, wipe them */
		entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
		if (!entry)
		{
			return NULL;
		}
		INIT_HLIST_NODE(&entry->node);
		hlist_add_head(&entry->node, head);
		memcpy(entry->data.mac, mac, ETH_ALEN);
		entry->data.ip = ip;
	}
	else if(entry->data.ip != ip)
	{
		clear_ip_nat(head, ip); /* if entries with new ip exist, wipe them */
		entry->data.ip = ip;
	}
	return entry;
}

#ifdef CONFIG_PROC_FS

static void *arpnat_start(struct seq_file *seq, loff_t *loff_pos)
{
	static unsigned long counter = 0;

	/* beginning a new sequence ? */
	if ( *loff_pos == 0 )
	{
		/* yes => return a non null value to begin the sequence */
		return &counter;
	}
	else
	{
		/* no => it's the end of the sequence, return end to stop reading */
		*loff_pos = 0;
		return NULL;
	}
}

static void *arpnat_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}


static void arpnat_stop(struct seq_file *seq, void *v)
{
	//don't need to do anything
}


static int arpnat_cache_show(struct seq_file *s, void *v)
{
	struct mac2ip* tpos;
	struct hlist_node* pos;
	struct hlist_node* n;
	unsigned long flags;

	spin_lock_irqsave(&arpnat_lock, flags);
	hlist_for_each_entry_safe(tpos, pos, n, &arpnat_table, node)
	{
		seq_printf(s, STRMAC"\t"STRIP"\n", MAC2STR(tpos->data.mac), IP2STR(tpos->data.ip));
	}
	spin_unlock_irqrestore(&arpnat_lock, flags);

	return 0;
}
static int arpnat_info_show(struct seq_file *s, void *v)
{
	seq_printf(s, "Debug: %d\nBOOTPNAT: %d\n", debug, bootpnat);
	return 0;
}


static struct seq_operations arpnat_cache_sops = {
	.start = arpnat_start,
	.next  = arpnat_next,
	.stop  = arpnat_stop,
	.show  = arpnat_cache_show
};
static struct seq_operations arpnat_info_sops = {
	.start = arpnat_start,
	.next  = arpnat_next,
	.stop  = arpnat_stop,
	.show  = arpnat_info_show
};

static int arpnat_cache_open(struct inode *inode, struct file* file)
{
	return seq_open(file, &arpnat_cache_sops);
}
static int arpnat_info_open(struct inode *inode, struct file* file)
{
	return seq_open(file, &arpnat_info_sops);
}


static struct file_operations arpnat_cache_fops = {
	.owner   = THIS_MODULE,
	.open    = arpnat_cache_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};
static struct file_operations arpnat_info_fops = {
	.owner   = THIS_MODULE,
	.open    = arpnat_info_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};



#endif


static unsigned int ebt_target_arpnat(struct sk_buff *pskb, const struct xt_action_param *par)
{
	const struct net_device *in  =  par->in;
	const struct net_device *out =  par->out;

	const struct ebt_nat_info *info = (struct ebt_nat_info *) par->targinfo;

	struct arphdr *ah = NULL;
	struct arphdr _arph;


	//used for target only
	uint8_t* eth_smac = eth_hdr(pskb)->h_source;
	uint8_t* eth_dmac = eth_hdr(pskb)->h_dest;
	uint32_t* arp_sip = NULL;
	uint8_t* arp_smac = NULL;
	uint32_t* arp_dip = NULL;
	uint8_t* arp_dmac = NULL;
	struct mac2ip* entry = NULL;
	unsigned long flags;

	/* if it's an arp packet, initialize pointers to arp source/dest ip/mac addresses in skb */
	if (eth_hdr(pskb)->h_proto == __constant_htons(ETH_P_ARP))
	{
		if(debug)
		{
			printk("ARPNAT ARP DETECTED\n");
		}
		ah = skb_header_pointer(pskb, 0, sizeof(_arph), &_arph);
		if (ah->ar_hln == ETH_ALEN && ah->ar_pro == htons(ETH_P_IP) && ah->ar_pln == 4)
		{
			unsigned char *raw = skb_network_header(pskb);
			arp_sip = (uint32_t*)(raw + sizeof(struct arphdr) + (arp_hdr(pskb)->ar_hln));
			arp_smac = raw + sizeof(struct arphdr);
			arp_dip = (uint32_t*)(raw + sizeof(struct arphdr) + (2*(arp_hdr(pskb)->ar_hln)) + arp_hdr(pskb)->ar_pln);
			arp_dmac = raw + sizeof(struct arphdr) + arp_hdr(pskb)->ar_hln + arp_hdr(pskb)->ar_pln;
		}
		else
		{
			ah = NULL;
		}
	}

	if (in)
	{
		struct net_bridge_port *in_br_port;
		in_br_port = br_port_get_rcu(in);

		/* handle input packets */
		if(debug)
		{
			printk("ARPNAT INBOUND DETECTED\n");
		}

		if (ah)
		{
			if(debug)
			{
				printk("IN ARPNAT:\n");
				printk("          arp_smac="STRMAC", arp_dmac="STRMAC"\n", MAC2STR(arp_smac), MAC2STR(arp_dmac));
				printk("          arp_sip ="STRIP", arp_dip ="STRIP"\n", IP2STR(*arp_sip), IP2STR(*arp_dip));
				if(ah->ar_op == __constant_htons(ARPOP_REPLY))
				{
					printk("          arp_op=reply\n");
				}
				else if(ah->ar_op == __constant_htons(ARPOP_REQUEST))
				{
					printk("          arp_op=request\n");
				}
				else
				{
					printk("          arp_op=%d\n", ntohs(ah->ar_op));
				}

			}


			if (inet_confirm_addr( __in_dev_get_rcu(in_br_port->br->dev) , 0, *arp_dip, RT_SCOPE_HOST))
			{
				if (debug)
				{
					printk("          TO US\n");
				}
				return info->target;
			}


			spin_lock_irqsave(&arpnat_lock, flags);
			entry = find_ip_nat(&arpnat_table, *arp_dip);
			switch (ah->ar_op)
			{
				case __constant_htons(ARPOP_REPLY):
				case __constant_htons(ARPOP_REQUEST):
				if (entry)
				{
					uint32_t dip = *arp_dip;
					uint32_t sip = inet_select_addr(in_br_port->br->dev, dip, RT_SCOPE_LINK);
					if (! (eth_dmac[0] & 1))
					{
						if (debug)
						{
							printk("          "STRMAC" -> "STRMAC"\n", MAC2STR(eth_dmac), MAC2STR(entry->data.mac));
						}
						memcpy(arp_dmac, entry->data.mac, ETH_ALEN);
						memcpy(eth_dmac, entry->data.mac, ETH_ALEN);
						(pskb)->pkt_type = (dip != sip) ? PACKET_OTHERHOST : (pskb)->pkt_type;
					}
					spin_unlock_irqrestore(&arpnat_lock, flags);
					/*if (dip != sip)
					{
						if (debug)
							printk("SEND ARP REQUEST: "STRIP" -> "STRIP"\n", IP2STR(sip), IP2STR(dip));
						arp_send(ARPOP_REQUEST, ETH_P_ARP, dip, &in_br_port->br->dev, sip, NULL, in_br_port->br->dev.dev_addr, NULL);
					}*/
					return info->target;
				}
				break;
			}
			spin_unlock_irqrestore(&arpnat_lock, flags);
		}
		else if (eth_hdr(pskb)->h_proto == __constant_htons(ETH_P_IP))
		{
			struct iphdr *iph = ip_hdr(pskb);
			struct udphdr *uh = NULL;
			if (bootpnat && (unsigned char)iph->protocol == (unsigned char)IPPROTO_UDP && !(iph->frag_off & htons(IP_OFFSET)))
			{
				uh = (struct udphdr*)((u_int32_t *)iph + iph->ihl);
				if(uh->dest == htons(67) || uh->dest == htons(68) )
				{
					//do something illegal for BOOTP
					uint32_t* giaddrp = (uint32_t*)(((uint8_t*)uh) + sizeof(*uh) + GIADDR_OFFSET);
					uint8_t* mac = (uint8_t*)(giaddrp + 1);
					uint32_t ihl = iph->ihl << 2;
					uint32_t size = (pskb)->len - ihl;
					uint32_t orig_daddr = iph->daddr;

					iph->daddr = 0xffffffff;
					if (debug)
					{
						printk("IN BOOTPRELAY: "STRMAC"["STRIP"] -> "STRMAC"["STRIP"]\n", MAC2STR(eth_dmac), IP2STR(orig_daddr), MAC2STR(mac), IP2STR(iph->daddr));
					}
					memcpy(eth_dmac, mac, ETH_ALEN);
					*giaddrp = 0;
					uh->dest = htons(68);
					iph->check = 0;
					uh->check = 0;
					iph->check = ip_fast_csum((uint8_t*)iph, iph->ihl);
					(pskb)->csum = csum_partial((uint8_t*)iph + ihl, size, 0);
					uh->check = csum_tcpudp_magic(iph->saddr, iph->daddr, size, iph->protocol, (pskb)->csum);

					if (uh->check == 0)
					{
						uh->check = 0xFFFF;
					}
					return info->target;
				}
				else
				{
					goto HANDLE_IP_PKT;
				}
			}
			else
			{
				HANDLE_IP_PKT:
				spin_lock_irqsave(&arpnat_lock, flags);
				entry = find_ip_nat(&arpnat_table, iph->daddr);
				if (entry)
				{
					if (inet_confirm_addr( __in_dev_get_rcu(in_br_port->br->dev),  0, entry->data.ip, RT_SCOPE_HOST))
					{
						//to me
						if (debug)
						{
							printk("IP PKT TO ME: "STRMAC"["STRIP"] -> "STRMAC"[type: %d]\n", MAC2STR(eth_dmac), IP2STR(iph->daddr), MAC2STR(in_br_port->br->dev->dev_addr), (pskb)->pkt_type);
						}
						memcpy(eth_dmac, in_br_port->br->dev->dev_addr, ETH_ALEN);
					}
					else
					{
						if (debug)
						{
							printk("IP PKT TO OTHER: "STRMAC"["STRIP"] -> "STRMAC"[type: %d]\n", MAC2STR(eth_dmac), IP2STR(iph->daddr), MAC2STR(entry->data.mac), (pskb)->pkt_type);
						}
						memcpy(eth_dmac, entry->data.mac, ETH_ALEN);
						(pskb)->pkt_type = PACKET_OTHERHOST;
					}
					spin_unlock_irqrestore(&arpnat_lock, flags);
					return info->target;
				}
				spin_unlock_irqrestore(&arpnat_lock, flags);
			}
		}

		if (! (eth_dmac[0] & 1))
		{
			if (memcmp(in_br_port->br->dev->dev_addr, eth_dmac, ETH_ALEN) && memcmp(in->dev_addr, eth_dmac, ETH_ALEN))
			{
				return EBT_DROP;
			}
			spin_lock_irqsave(&arpnat_lock, flags);
			entry = find_mac_nat(&arpnat_table, eth_dmac);
			if (entry)
			{
				memcpy(eth_dmac, entry->data.mac, ETH_ALEN);
			}
			else
			{
				memcpy(eth_dmac, in_br_port->br->dev->dev_addr, ETH_ALEN);
			}
			spin_unlock_irqrestore(&arpnat_lock, flags);
		}
	}
	else if (out)
	{
		struct net_bridge_port *out_br_port;
		out_br_port = br_port_get_rcu(out);


		/* handle outbound packets */
		if (ah)
		{
			switch (ah->ar_op)
			{
				case __constant_htons(ARPOP_REQUEST):
				case __constant_htons(ARPOP_REPLY):


				/* do BR ip lookup */
				if(inet_confirm_addr( __in_dev_get_rcu(out_br_port->br->dev), 0, *arp_dip, RT_SCOPE_HOST))
				{
					return info->target;
				}
				if(!inet_confirm_addr( __in_dev_get_rcu(out_br_port->br->dev), 0, *arp_sip, RT_SCOPE_HOST))
				{
					spin_lock_irqsave(&arpnat_lock, flags);
					update_arp_nat(&arpnat_table, arp_smac, *arp_sip);
					spin_unlock_irqrestore(&arpnat_lock, flags);
				}

				//pskb = skb_unshare(pskb, GFP_ATOMIC);
				eth_smac = eth_hdr(pskb)->h_source;
				arp_smac = skb_network_header(pskb) + sizeof(struct arphdr);
				if (debug)
				{
					printk("OUT ARPNAT: "STRMAC" -> "STRMAC"\n", MAC2STR(eth_smac), MAC2STR(out->dev_addr));
					printk("           arp_smac="STRMAC", arp_dmac="STRMAC"\n", MAC2STR(arp_smac), MAC2STR(arp_dmac));
					printk("           arp_sip ="STRIP", arp_dip ="STRIP"\n", IP2STR(*arp_sip), IP2STR(*arp_dip));
					if(ah->ar_op == __constant_htons(ARPOP_REPLY))
					{
						printk("           arp_op=reply\n");
					}
					else if(ah->ar_op == __constant_htons(ARPOP_REQUEST))
					{
						printk("           arp_op=request\n");
					}
					else
					{
						printk("           arp_op=%d\n", ntohs(ah->ar_op));
					}
				}
				memcpy(arp_smac, out->dev_addr, ETH_ALEN);
				memcpy(eth_smac, out->dev_addr, ETH_ALEN);
				return info->target;
				break;
			}
		}
		else if (bootpnat && eth_hdr(pskb)->h_proto == __constant_htons(ETH_P_IP) && memcmp(out_br_port->br->dev->dev_addr, eth_smac, ETH_ALEN))
		{
			struct iphdr *iph = ip_hdr(pskb);
			struct udphdr *uh = NULL;
			if ( (unsigned char)iph->protocol == (unsigned char)IPPROTO_UDP && !(iph->frag_off & htons(IP_OFFSET)))
			{
				uh = (struct udphdr*)((u_int32_t *)iph + iph->ihl);
				if (uh->dest == htons(67) || uh->dest == htons(68) )
				{
					//do something illegal for BOOTP
					uint32_t giaddr = inet_select_addr(out_br_port->br->dev, iph->daddr, RT_SCOPE_LINK);
					uint32_t* giaddrp = (uint32_t*)(((uint8_t*)uh) + sizeof(*uh) + GIADDR_OFFSET);
					uint32_t ihl = iph->ihl << 2;
					uint32_t size = (pskb)->len - ihl;
					if (debug)
					{
						printk("OUT BOOTPRELAY: "STRIP" -> "STRIP"\n", IP2STR(*giaddrp), IP2STR(giaddr));
					}
					*giaddrp = giaddr;
					uh->check = 0;
					(pskb)->csum = csum_partial((uint8_t*)iph + ihl, size, 0);
					uh->check = csum_tcpudp_magic(iph->saddr, iph->daddr, size, iph->protocol, (pskb)->csum);

					if (uh->check == 0)
					{
						uh->check = 0xFFFF;
					}
				}
			}
		}
		memcpy(eth_smac, out->dev_addr, ETH_ALEN);
	}
	return info->target;
}

static int ebt_target_nat_arpcheck(const struct xt_tgchk_param *par)
{
	return 0;
}
static struct xt_target arpnat =
{
	.name		= EBT_ARPNAT_TARGET,
	.revision	= 0,
	.family		= NFPROTO_BRIDGE,
	.table		= "nat",
	.hooks		= (1 << NF_BR_NUMHOOKS) | (1 << NF_BR_POST_ROUTING) |  (1 << NF_BR_PRE_ROUTING) ,
	.target		= ebt_target_arpnat,
	.checkentry	= ebt_target_nat_arpcheck,
	.targetsize	= XT_ALIGN(sizeof(struct ebt_nat_info)),
	.me		= THIS_MODULE
};

static int __init init(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *proc_arpnat_info  = create_proc_entry("arpnat_info", 0, NULL);
	struct proc_dir_entry *proc_arpnat_cache = create_proc_entry("arpnat_cache", 0, NULL);
	if(proc_arpnat_info)
	{
		proc_arpnat_info->proc_fops = &arpnat_info_fops;
	}

	if(proc_arpnat_cache)
	{
		proc_arpnat_cache->proc_fops = &arpnat_cache_fops;
	}
#endif
	return xt_register_target(&arpnat);
}

static void __exit fini(void)
{
	xt_unregister_target(&arpnat);
	free_arp_nat(&arpnat_table);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("arpnat_info", NULL);
	remove_proc_entry("arpnat_cache", NULL);
#endif
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");


