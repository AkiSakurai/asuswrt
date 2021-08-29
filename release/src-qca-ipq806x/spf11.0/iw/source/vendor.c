#include <errno.h>
#include <string.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

SECTION(vendor);

static int print_vendor_response(struct nl_msg *msg, void *arg)
{
	struct nlattr *attr;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	bool print_ascii = (bool) arg;
	uint8_t *data;
	int len;

	attr = nla_find(genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0),
			NL80211_ATTR_VENDOR_DATA);
	if (!attr) {
		fprintf(stderr, "vendor data attribute missing!\n");
		return NL_SKIP;
	}

	data = (uint8_t *) nla_data(attr);
	len = nla_len(attr);

	if (print_ascii)
		iw_hexdump("vendor response", data, len);
	else
		fwrite(data, 1, len, stdout);

	return NL_OK;
}

static int read_file(FILE *file, char *buf, size_t size)
{
	size_t count = 0;
	int data;

	while ((data = fgetc(file)) != EOF) {
		if (count >= size)
			return -EINVAL;
		buf[count] = data;
		count++;
	}

	return count;
}

static int read_hex(unsigned int argc, char **argv, char *buf, size_t size)
{
	unsigned int i, data;
	int res;

	if (argc > size)
		return -EINVAL;

	for (i = 0; i < argc; i++) {
		res = sscanf(argv[i], "0x%x", &data);
		if (res != 1 || data > 0xff)
			return -EINVAL;
		buf[i] = data;
	}

	return argc;
}

static int handle_vendor(struct nl80211_state *state,
			 struct nl_msg *msg, int argc, char **argv,
			 enum id_input id)
{
	unsigned int oui;
	unsigned int subcmd;
	struct nlattr *params;
	char buf[2048] = {};
	int res, count = 0, i = 0;
	FILE *file = NULL;
	char *end;
	unsigned int coex, prio_mask, weight, ap_ps;
	bool wlan_prio_config = false;
	struct nlattr *wlan_prio;
	struct nl_msg *prio_msg;

	if (argc < 3)
		return 1;

	res = sscanf(argv[0], "0x%x", &oui);
	if (res != 1) {
		printf("Vendor command must start with 0x\n");
		return 2;
	}

	res = sscanf(argv[1], "0x%x", &subcmd);
	if (res != 1) {
		printf("Sub command must start with 0x\n");
		return 2;
	}

	if (!strcmp(argv[2], "-"))
		file = stdin;
	else
		file = fopen(argv[2], "r");

	NLA_PUT_U32(msg, NL80211_ATTR_VENDOR_ID, oui);
	NLA_PUT_U32(msg, NL80211_ATTR_VENDOR_SUBCMD, subcmd);

	if (subcmd == 0xb6)
		goto coex_config;

	if (subcmd == 0x4a) {
		argc -= 2;
		argv += 2;

		if (!argc)
			return 1;

		params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);

		if (!strcmp(argv[0], "ap-ps")) {
			if (argc < 2)
				return 1;
			ap_ps = strtol(argv[1], &end, 10);
			if (*end)
				return 2;

			if (ap_ps > 1)
				return -EINVAL;

			if (ap_ps)
				NLA_PUT_FLAG(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_GTX);
		}

		nla_nest_end(msg, params);
		return 0;
	}

	if (file) {
		count = read_file(file, buf, sizeof(buf));
		fclose(file);
	} else
		count = read_hex(argc - 2, &argv[2], buf, sizeof(buf));

	if (count < 0)
		return -EINVAL;

	if (count > 0)
		NLA_PUT(msg, NL80211_ATTR_VENDOR_DATA, count, buf);

coex_config:
	argc -= 2;
	argv += 2;

	if (!argc)
		return 1;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);

	if (!strcmp(argv[0], "coex")) {
		if (argc < 2)
			return 1;
		coex = strtol(argv[1], &end, 10);
		if (*end) {
			return 2;
		}
		NLA_PUT_U8(msg, QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_ENABLE,
			   coex);
		argv += 2;
		argc -= 2;
	}

	if (!argc) {
		nla_nest_end(msg, params);
		return 0;
	}

	prio_msg = nlmsg_alloc();
	if (!prio_msg)
		return -ENOMEM;

	if (!strcmp(argv[0], "wlan_prio")) {
		argc--;
		argv++;

		while(argc) {
			wlan_prio = nla_nest_start(prio_msg, i);
			if (!wlan_prio)
				goto nla_put_failure;

			prio_mask = strtol(argv[0], &end, 10);
			if (*end) {
				return 2;
			}

			NLA_PUT_U8(prio_msg, QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_MASK,
				   prio_mask);
			wlan_prio_config = true;
			argc--;
			argv++;
			if (!argc) {
				nla_nest_end(prio_msg, wlan_prio);
				goto out;
			}

			weight = strtol(argv[0], &end, 10);
			if (*end)
				return 2;

			if (weight > 255)
				return 1;

			NLA_PUT_U8(prio_msg, QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_WEIGHT,
				   weight);
			nla_nest_end(prio_msg, wlan_prio);
			i++;
			argc--;
			argv++;
		}
	}

out:
	if (wlan_prio_config)
		nla_put_nested(msg, QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_WLAN_PRIORITY,
			       prio_msg);
	nla_nest_end(msg, params);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}

static int handle_vendor_recv(struct nl80211_state *state,
			      struct nl_msg *msg, int argc,
			      char **argv, enum id_input id)
{
	register_handler(print_vendor_response, (void *) true);
	return handle_vendor(state, msg, argc, argv, id);
}

static int handle_vendor_recv_bin(struct nl80211_state *state,
				  struct nl_msg *msg, int argc,
				  char **argv, enum id_input id)
{
	register_handler(print_vendor_response, (void *) false);
	return handle_vendor(state, msg, argc, argv, id);
}

COMMAND(vendor, send, "<oui> <subcmd> <filename|-|hex data>", NL80211_CMD_VENDOR, 0, CIB_NETDEV, handle_vendor, "");
COMMAND(vendor, recv, "<oui> <subcmd> <filename|-|hex data>", NL80211_CMD_VENDOR, 0, CIB_NETDEV, handle_vendor_recv, "");
COMMAND(vendor, recvbin, "<oui> <subcmd> <filename|-|hex data>", NL80211_CMD_VENDOR, 0, CIB_NETDEV, handle_vendor_recv_bin, "");
