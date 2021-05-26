#include <linux/version.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/spinlock.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#include <net/netfilter/nf_conntrack.h>
#else
#include <linux/netfilter_ipv4/ip_conntrack.h>
#endif

#include "tdts_udb.h"

#define QOS_NLMSG_TYPE 0x905
#define QOS_NLMSG_ACK  0x906
#define QOS_NLMSG_NACK 0x907

#define TC_DBG_ENABLE 0

#undef TC_ERR
#define TC_ERR(fmt, args...) \
	printk(KERN_ERR "[%s:%d] " fmt, __FUNCTION__, __LINE__, ##args);

#undef TC_DBG
#if TC_DBG_ENABLE
#define TC_DBG(fmt, args...) \
	printk(KERN_INFO "[%s:%d] " fmt, __FUNCTION__, __LINE__, ##args);
#else
#define TC_DBG(fmt, args...) do { } while (0)
#endif

typedef struct {
	char cmd[256];
	struct list_head list;
} tc_cmd_q_t;

#define TC_CMD_TIMEOUT 3

#define TC_CMD_MAX_RETRY 5 // I added this, though I don't think it helps much. --mit

static struct timer_list tc_cmd_q_timer;
static struct list_head tc_cmd_q_list;
static DEFINE_RWLOCK(tc_cmd_q_lock);
static atomic_t tc_cmd_deqing = ATOMIC_INIT(0);

#ifdef TC_CMD_MAX_RETRY
static char tc_current_cmd[256] = {0};
static int tc_cmd_retry_cnt = 0;
#endif

static struct sock *qos_cmd_nl_sock = NULL;
static int tcd_pid = 0;

static int tc_do_cmd(char *cmd);

static void tc_cmd_deq(void)
{
	write_lock_bh(&tc_cmd_q_lock);

	if (!list_empty(&tc_cmd_q_list))
	{
		tc_cmd_q_t *first;
		atomic_set(&tc_cmd_deqing, 1);

		if ((first = list_first_entry(&tc_cmd_q_list, tc_cmd_q_t, list)))
		{
			tc_do_cmd(first->cmd);
#ifdef TC_CMD_MAX_RETRY
			snprintf(tc_current_cmd, sizeof(tc_current_cmd), "%s", first->cmd);
#endif
			list_del(&first->list);
			kfree(first);
		}

		/* when deqing, set the timer to wait TC_CMD_TIMEOUT secs,
		 * if there's no ack from userland */
		mod_timer(&tc_cmd_q_timer, jiffies + (HZ * TC_CMD_TIMEOUT));
	}
	else
	{
		TC_DBG("Q is empty%s\n", atomic_read(&tc_cmd_deqing) ? ", stop deQ" : "");
		atomic_set(&tc_cmd_deqing, 0);

		/* Q is empty, stop the timer */
		del_timer(&tc_cmd_q_timer);
	}

	write_unlock_bh(&tc_cmd_q_lock);	
}

static void tc_cmd_timeout(unsigned long data)
{
	TC_DBG("Timeout!%s\n", atomic_read(&tc_cmd_deqing) ? " force deQ" : " start deQ");
	tc_cmd_deq();
}

int tc_add_cmd(char *cmd)
{
	int cmd_len = strlen(cmd) + 1;
	tc_cmd_q_t *cmd_q = NULL;

	cmd_q = kmalloc(sizeof(tc_cmd_q_t), GFP_ATOMIC);

	if (!cmd_q)
	{
		return -1;
	}

	memcpy(cmd_q->cmd, cmd, cmd_len - 1);
	cmd_q->cmd[cmd_len - 1] = 0;

	TC_DBG("New cmd received\n");

	write_lock_bh(&tc_cmd_q_lock);

	list_add_tail(&cmd_q->list, &tc_cmd_q_list);

	/* start timer if cmd is added to Q */
	mod_timer(&tc_cmd_q_timer, jiffies + (HZ * 1));

	write_unlock_bh(&tc_cmd_q_lock);

	return 0;
}

static int tc_do_cmd(char *cmd)
{
	int cmd_len = strlen(cmd) + 1;
	int size;
	struct sk_buff *nskb = NULL;
	struct nlmsghdr *nlh = NULL;
	uint8_t *data = NULL;

	if (!tcd_pid)
	{
		return -1;
	}
	
	size = NLMSG_SPACE(cmd_len);

	nskb =	alloc_skb(size, GFP_ATOMIC);
	if (unlikely(!nskb))
	{
		TC_ERR("Failed to alloc. SKB.\n");
		return 1;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
	nlh = nlmsg_put(nskb, 0, 0, QOS_NLMSG_TYPE, size - sizeof(struct nlmsghdr), 0);
#else
	nlh = NLMSG_PUT(nskb, 0, 0, QOS_NLMSG_TYPE, size - sizeof(struct nlmsghdr));
#endif

	if (unlikely(!nlh))
	{
		TC_DBG("nlmsg_failure\n");
		goto nlmsg_failure;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
	data = (void *)nlmsg_data(nlh);
#else
	data = (void *)NLMSG_DATA(nlh);
#endif

	memcpy(data, cmd, cmd_len);

	netlink_unicast(qos_cmd_nl_sock, nskb, tcd_pid, MSG_DONTWAIT);

	return 0;

nlmsg_failure:
	if (nskb)
	{
		kfree_skb(nskb);
	}

	return -1;
}

static void this_rcv(struct sk_buff *skb)
{
//	TC_DBG("in this_rcv skb->len=%d\n", skb->len);
	if (skb->len >= sizeof(struct nlmsghdr) && skb->data)
	{
		struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
//		uint8_t *msg = (uint8_t *)NLMSG_DATA(nlh);
//		int len = nlh->nlmsg_len - sizeof(struct nlmsghdr);
		
		if (nlh->nlmsg_type == QOS_NLMSG_TYPE)
		{
			tcd_pid = nlh->nlmsg_pid;
			TC_DBG("set pid = %d\n", tcd_pid);
			/* register qos_ops */
			udb_shell_register_qos_ops(tc_add_cmd);
		}
		else if (nlh->nlmsg_type == QOS_NLMSG_ACK)
		{
			if (tcd_pid == nlh->nlmsg_pid)
			{
				TC_DBG("ACK received, continue deQ\n");
				tc_cmd_deq();
			}
		}
		else if (nlh->nlmsg_type == QOS_NLMSG_NACK)
		{
			if (tcd_pid == nlh->nlmsg_pid)
			{
#ifdef TC_CMD_MAX_RETRY
				TC_DBG("NACK received, retry cmd \"%s\"\n", tc_current_cmd);
				if (TC_CMD_MAX_RETRY > tc_cmd_retry_cnt++)
				{
					tc_do_cmd(tc_current_cmd);
					mod_timer(&tc_cmd_q_timer, jiffies + (HZ * TC_CMD_TIMEOUT));
				}
				else
				{
					TC_DBG("Retry failed %d times, continue deQ\n");
					tc_cmd_retry_cnt = 0;
					tc_cmd_deq();
				}
#else
				TC_DBG("NACK received, continue deQ\n");
				tc_cmd_deq();
#endif
			}
		}
	}
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
static void this_rcv_sk(struct sock *sk, int len)
{
	struct sk_buff *skb = skb_dequeue(&sk->sk_receive_queue);

	while (skb && skb->len)
	{
		this_rcv(skb);
		kfree_skb(skb);
		skb = skb_dequeue(&sk->sk_receive_queue);
	}
}
#endif

int qos_cmd_init(void)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22))
	struct net *init_net_ns = &(init_net);

	if (!init_net_ns)
	{
		TC_ERR("Cannot use init's network namespace!\n");
		return -ENOENT;
	}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
	{
		struct netlink_kernel_cfg cfg = {
		    .input = this_rcv,
		};
		qos_cmd_nl_sock = netlink_kernel_create(
			init_net_ns, TMCFG_APP_K_TDTS_UDBFW_QOS_NETLINK_ID, &cfg);
	}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	qos_cmd_nl_sock = netlink_kernel_create(
		init_net_ns, TMCFG_APP_K_TDTS_UDBFW_QOS_NETLINK_ID, 0, this_rcv, NULL, THIS_MODULE);
#else
	qos_cmd_nl_sock = netlink_kernel_create(
		TMCFG_APP_K_TDTS_UDBFW_QOS_NETLINK_ID, 0, this_rcv_sk, NULL, THIS_MODULE);
#endif

	if (!qos_cmd_nl_sock)
	{
		TC_ERR("Cannot create netlink sock.\n");
		return -EBUSY;
	}

	INIT_LIST_HEAD(&tc_cmd_q_list);

	init_timer(&tc_cmd_q_timer);
	setup_timer(&tc_cmd_q_timer, tc_cmd_timeout, 0);

	TC_DBG("qos_cmd_nl_sock=%p\n", qos_cmd_nl_sock);

	/* don't register qos_ops until tcd start and call-in this_rcv() --mit */
	
	return 0;
}

void qos_cmd_exit(void)
{
	if (qos_cmd_nl_sock)
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
		netlink_kernel_release(qos_cmd_nl_sock);
#else
		sock_release(qos_cmd_nl_sock->sk_socket);
#endif
	}

	udb_shell_unregister_qos_ops();

	del_timer(&tc_cmd_q_timer);
}
