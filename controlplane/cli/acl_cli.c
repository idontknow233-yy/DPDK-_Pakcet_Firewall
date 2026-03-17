#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cmdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_rdline.h>
#include <cmdline_socket.h>

#include <rte_eal.h>
#include <rte_ring.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>

#include "acl/acl.h"
#include "acl/acl6.h"
#include "ipc/acl_ipc.h"
#include "ipc/acl_hit_ipc.h"
#include "ipc/acl6_ipc.h"
#include "ipc/acl6_hit_ipc.h"
#include "ipc/session_ipc.h"
#include "ipc/session6_ipc.h"
#include "ipc/stats_ipc.h"
#include "ipc/rlim_ipc.h"
#include "ipc/route6_ipc.h"
#include "ipc/attack_ipc.h"

static volatile sig_atomic_t force_quit;
static struct rte_ring *acl_cmd_ring;
static struct rte_ring *acl_resp_ring;
static struct acl_shared_cfg *acl_shared_cfg;
static struct acl_hit_shared_cfg *acl_hit_shared_cfg;
static struct rte_ring *acl6_cmd_ring;
static struct rte_ring *acl6_resp_ring;
static struct acl6_shared_cfg *acl6_shared_cfg;
static struct acl6_hit_shared_cfg *acl6_hit_shared_cfg;
static struct session_shared_cfg *session_shared_cfg;
static struct session6_shared_cfg *session6_shared_cfg;
static struct portstats_shared_cfg *portstats_shared_cfg;
static struct denylog_shared_cfg *denylog_shared_cfg;
static struct denylog6_shared_cfg *denylog6_shared_cfg;
static struct rlim_shared_cfg *rlim_shared_cfg;
static struct route6_shared_cfg *route6_shared_cfg;
static struct attack_shared_cfg *attack_shared_cfg;
static const char *cli_host = "0.0.0.0";
static uint16_t cli_port = 8086;
static uint32_t cmd_seq;

static const char welcome[] =
	"\n"
	"Welcome to IP Pipeline!\n"
	"\n";
static const char prompt[] = "pipeline> ";

static void signal_handler(int signum) {
	if (signum == SIGINT || signum == SIGTERM) {
		force_quit = 1;
	}
}

struct cmd_acl_add_result {
	cmdline_fixed_string_t acl;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t action;
	cmdline_ipaddr_t src;
	cmdline_ipaddr_t dst;
	uint8_t proto;
	uint16_t src_port_min;
	uint16_t src_port_max;
	uint16_t dst_port_min;
	uint16_t dst_port_max;
};

struct cmd_acl_del_result {
	cmdline_fixed_string_t acl;
	cmdline_fixed_string_t del;
	uint32_t index;
};

struct cmd_acl_clear_result {
	cmdline_fixed_string_t acl;
	cmdline_fixed_string_t clear;
};

struct cmd_acl_list_result {
	cmdline_fixed_string_t acl;
	cmdline_fixed_string_t list;
};

struct cmd_acl_hits_result {
	cmdline_fixed_string_t acl;
	cmdline_fixed_string_t hits;
};

struct cmd_acl6_hits_result {
	cmdline_fixed_string_t acl6;
	cmdline_fixed_string_t hits;
};

struct cmd_acl6_add_result {
	cmdline_fixed_string_t acl6;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t action;
	cmdline_ipaddr_t src;
	cmdline_ipaddr_t dst;
	uint8_t proto;
	uint16_t src_port_min;
	uint16_t src_port_max;
	uint16_t dst_port_min;
	uint16_t dst_port_max;
};

struct cmd_acl6_del_result {
	cmdline_fixed_string_t acl6;
	cmdline_fixed_string_t del;
	uint32_t index;
};

struct cmd_acl6_clear_result {
	cmdline_fixed_string_t acl6;
	cmdline_fixed_string_t clear;
};

struct cmd_acl6_list_result {
	cmdline_fixed_string_t acl6;
	cmdline_fixed_string_t list;
};

struct cmd_acl_quit_result {
	cmdline_fixed_string_t quit;
};

struct cmd_session_list_result {
	cmdline_fixed_string_t session;
	cmdline_fixed_string_t list;
	uint32_t limit;
};

struct cmd_session6_list_result {
	cmdline_fixed_string_t session6;
	cmdline_fixed_string_t list;
	uint32_t limit;
};

struct cmd_port_stats_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t stats;
};

struct cmd_deny_list_result {
	cmdline_fixed_string_t deny;
	cmdline_fixed_string_t list;
	uint32_t limit;
};

struct cmd_deny6_list_result {
	cmdline_fixed_string_t deny6;
	cmdline_fixed_string_t list;
	uint32_t limit;
};

struct cmd_ifcfg6_show_result {
	cmdline_fixed_string_t ifcfg6;
	cmdline_fixed_string_t show;
};

struct cmd_ifcfg6_set_result {
	cmdline_fixed_string_t ifcfg6;
	cmdline_fixed_string_t set;
	uint32_t port;
	cmdline_fixed_string_t cidr;
};

struct cmd_ifcfg6_clear_result {
	cmdline_fixed_string_t ifcfg6;
	cmdline_fixed_string_t clear;
	uint32_t port;
};

struct cmd_route6_list_result {
	cmdline_fixed_string_t route6;
	cmdline_fixed_string_t list;
};

struct cmd_route6_clear_result {
	cmdline_fixed_string_t route6;
	cmdline_fixed_string_t clear;
};

struct cmd_route6_del_result {
	cmdline_fixed_string_t route6;
	cmdline_fixed_string_t del;
	uint32_t index;
};

struct cmd_route6_add_result {
	cmdline_fixed_string_t route6;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t dst;
	cmdline_fixed_string_t nh;
	uint32_t port;
};

struct cmd_attack_show_result {
	cmdline_fixed_string_t attack;
	cmdline_fixed_string_t show;
};

struct cmd_attack_set_result {
	cmdline_fixed_string_t attack;
	cmdline_fixed_string_t set;
	uint32_t mitigation;
	uint32_t scan_ports_sec;
	uint32_t ban_sec;
};

struct cmd_ddos_show_result {
	cmdline_fixed_string_t ddos;
	cmdline_fixed_string_t show;
};

struct cmd_ddos_set_result {
	cmdline_fixed_string_t ddos;
	cmdline_fixed_string_t set;
	uint32_t syn_pps;
	uint32_t syn_burst;
	uint32_t udp_pps;
	uint32_t udp_burst;
};

static int parse_ipaddr_v4(const cmdline_ipaddr_t *in, uint32_t *ip, uint32_t *mask) {
	if (in->family != AF_INET) {
		return -EINVAL;
	}
	*ip = rte_be_to_cpu_32(in->addr.ipv4.s_addr);
	if (in->prefixlen > 32) {
		return -EINVAL;
	}
	if (in->prefixlen == 0) {
		*mask = 0;
	} else {
		*mask = 0xFFFFFFFFu << (32 - in->prefixlen);
	}
	return 0;
}

static void print_ip_mask(char *out, size_t len, uint32_t ip, uint32_t mask) {
	struct in_addr addr;
	addr.s_addr = rte_cpu_to_be_32(ip);
	char ipbuf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr, ipbuf, sizeof(ipbuf));
	uint32_t depth = mask ? (uint32_t)__builtin_popcount(mask) : 0;
	snprintf(out, len, "%s/%u", ipbuf, depth);
}

static void print_ipv4(char *out, size_t len, uint32_t ip) {
	struct in_addr addr;
	addr.s_addr = rte_cpu_to_be_32(ip);
	inet_ntop(AF_INET, &addr, out, len);
}

static void print_ipv6(char *out, size_t len, const struct rte_ipv6_addr *ip) {
	struct in6_addr a6;
	memcpy(&a6, ip, sizeof(a6));
	inet_ntop(AF_INET6, &a6, out, len);
}

static void print_ipv6_cidr(char *out, size_t len, const struct rte_ipv6_addr *ip, uint8_t depth) {
	char ipbuf[INET6_ADDRSTRLEN];
	print_ipv6(ipbuf, sizeof(ipbuf), ip);
	snprintf(out, len, "%s/%u", ipbuf, depth);
}

static int parse_ipaddr_v6(const cmdline_ipaddr_t *in, struct rte_ipv6_addr *ip, uint8_t *depth) {
	if (!in || !ip || !depth) {
		return -EINVAL;
	}
	if (in->family != AF_INET6) {
		return -EINVAL;
	}
	if (in->prefixlen > 128) {
		return -EINVAL;
	}
	memcpy(ip, &in->addr.ipv6, sizeof(*ip));
	*depth = (uint8_t)in->prefixlen;
	return 0;
}

static int wait_resp(uint32_t seq, struct acl_cmd_resp **resp_out) {
	const int max_wait = 2000;
	for (int i = 0; i < max_wait; i++) {
		struct acl_cmd_resp *resp = NULL;
		if (rte_ring_dequeue(acl_resp_ring, (void **)&resp) == 0) {
			if (resp->seq == seq) {
				*resp_out = resp;
				return 0;
			}
			rte_free(resp);
		}
		rte_delay_us_sleep(1000);
	}
	return -1;
}

static int send_cmd_and_wait(struct acl_cmd_msg *cmd, struct acl_cmd_resp **resp_out) {
	if (rte_ring_enqueue(acl_cmd_ring, cmd) != 0) {
		return -1;
	}
	return wait_resp(cmd->seq, resp_out);
}

static int wait_resp6(uint32_t seq, struct acl6_cmd_resp **resp_out) {
	const int max_wait = 2000;
	for (int i = 0; i < max_wait; i++) {
		struct acl6_cmd_resp *resp = NULL;
		if (rte_ring_dequeue(acl6_resp_ring, (void **)&resp) == 0) {
			if (resp->seq == seq) {
				*resp_out = resp;
				return 0;
			}
			rte_free(resp);
		}
		rte_delay_us_sleep(1000);
	}
	return -1;
}

static int send_cmd_and_wait6(struct acl6_cmd_msg *cmd, struct acl6_cmd_resp **resp_out) {
	if (rte_ring_enqueue(acl6_cmd_ring, cmd) != 0) {
		return -1;
	}
	return wait_resp6(cmd->seq, resp_out);
}

static void cmd_acl_add_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_acl_add_result *res = parsed_result;
	struct acl_rule rule;
	uint32_t src_ip;
	uint32_t src_mask;
	uint32_t dst_ip;
	uint32_t dst_mask;

	if (parse_ipaddr_v4(&res->src, &src_ip, &src_mask) != 0 ||
	    parse_ipaddr_v4(&res->dst, &dst_ip, &dst_mask) != 0) {
		cmdline_printf(cl, "Only IPv4 is supported\n");
		return;
	}
	if (res->src_port_min > res->src_port_max || res->dst_port_min > res->dst_port_max) {
		cmdline_printf(cl, "Invalid port range\n");
		return;
	}

	memset(&rule, 0, sizeof(rule));
	rule.src_ip = src_ip;
	rule.src_mask = src_mask;
	rule.dst_ip = dst_ip;
	rule.dst_mask = dst_mask;
	rule.proto = res->proto;
	rule.src_port_min = res->src_port_min;
	rule.src_port_max = res->src_port_max;
	rule.dst_port_min = res->dst_port_min;
	rule.dst_port_max = res->dst_port_max;
	rule.match_ports = (rule.proto != 0 || rule.src_port_max != 0 || rule.dst_port_max != 0);
	rule.allow = strcmp(res->action, "allow") == 0 ? 1 : 0;

	struct acl_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL_CMD_ADD;
	cmd->seq = ++cmd_seq;
	cmd->rule = rule;

	struct acl_cmd_resp *resp = NULL;
	if (send_cmd_and_wait(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL add failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL add failed\n");
	} else {
		cmdline_printf(cl, "ACL rule added\n");
	}
	rte_free(resp);
}

static void cmd_acl_del_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_acl_del_result *res = parsed_result;
	struct acl_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL_CMD_DEL;
	cmd->seq = ++cmd_seq;
	cmd->index = res->index;

	struct acl_cmd_resp *resp = NULL;
	if (send_cmd_and_wait(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL delete failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL delete failed\n");
	} else {
		cmdline_printf(cl, "ACL rule deleted\n");
	}
	rte_free(resp);
}

static void cmd_acl_clear_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	struct acl_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL_CMD_CLEAR;
	cmd->seq = ++cmd_seq;

	struct acl_cmd_resp *resp = NULL;
	if (send_cmd_and_wait(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL clear failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL clear failed\n");
	} else {
		cmdline_printf(cl, "ACL cleared\n");
	}
	rte_free(resp);
}

static int read_shared_rules(struct acl_rule *rules, uint32_t *count, uint64_t *version, uint64_t target_version) {
	if (!acl_shared_cfg || !rules || !count || !version) {
		return -1;
	}
	for (int i = 0; i < 200; i++) {
		uint64_t v1 = rte_atomic64_read(&acl_shared_cfg->version);
		uint32_t c = acl_shared_cfg->count;
		if (c > ACL_MAX_RULES) {
			c = ACL_MAX_RULES;
		}
		memcpy(rules, acl_shared_cfg->rules, sizeof(struct acl_rule) * c);
		uint64_t v2 = rte_atomic64_read(&acl_shared_cfg->version);
		if (v1 == v2 && (target_version == 0 || v2 == target_version)) {
			*count = c;
			*version = v2;
			return 0;
		}
		rte_delay_us_sleep(1000);
	}
	return -1;
}

static void print_acl_rule(struct cmdline *cl, const struct acl_cmd_resp *item) {
	char srcbuf[64];
	char dstbuf[64];
	print_ip_mask(srcbuf, sizeof(srcbuf), item->rule.src_ip, item->rule.src_mask);
	print_ip_mask(dstbuf, sizeof(dstbuf), item->rule.dst_ip, item->rule.dst_mask);
	if (item->rule.match_ports) {
		cmdline_printf(cl, "%u %s src=%s dst=%s proto=%u sport=%u-%u dport=%u-%u\n",
			item->rule_index, item->rule.allow ? "allow" : "deny",
			srcbuf, dstbuf, item->rule.proto,
			item->rule.src_port_min, item->rule.src_port_max,
			item->rule.dst_port_min, item->rule.dst_port_max);
	} else {
		cmdline_printf(cl, "%u %s src=%s dst=%s proto=any ports=any\n",
			item->rule_index, item->rule.allow ? "allow" : "deny", srcbuf, dstbuf);
	}
}

static void cmd_acl_list_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	struct acl_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL_CMD_LIST;
	cmd->seq = ++cmd_seq;

	struct acl_cmd_resp *resp = NULL;
	if (send_cmd_and_wait(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL list failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL list failed (cmd status)\n");
		rte_free(resp);
		return;
	}
	uint64_t target_version = resp->version;
	rte_free(resp);

	struct acl_rule rules[ACL_MAX_RULES];
	uint32_t count = 0;
	uint64_t version = 0;
	if (read_shared_rules(rules, &count, &version, target_version) != 0) {
		if (read_shared_rules(rules, &count, &version, 0) != 0) {
			cmdline_printf(cl, "ACL list failed (shared cfg)\n");
			return;
		}
	}
	cmdline_printf(cl, "ACL rules: %u (version %" PRIu64 ")\n", count, version);
	for (uint32_t i = 0; i < count; i++) {
		struct acl_cmd_resp item;
		memset(&item, 0, sizeof(item));
		item.rule = rules[i];
		item.rule_index = i;
		print_acl_rule(cl, &item);
	}
}

static void cmd_acl_hits_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!acl_hit_shared_cfg) {
		cmdline_printf(cl, "ACLHITS: count=0 rule_version=0 (version 0)\n");
		return;
	}
	uint64_t v1 = rte_atomic64_read(&acl_hit_shared_cfg->version);
	uint32_t count = acl_hit_shared_cfg->count;
	uint64_t rv = acl_hit_shared_cfg->rule_version;
	uint64_t pkts[ACL_MAX_RULES];
	uint64_t bytes[ACL_MAX_RULES];
	if (count > ACL_MAX_RULES) {
		count = ACL_MAX_RULES;
	}
	memcpy(pkts, acl_hit_shared_cfg->deny_pkts, sizeof(pkts));
	memcpy(bytes, acl_hit_shared_cfg->deny_bytes, sizeof(bytes));
	rte_rmb();
	uint64_t v2 = rte_atomic64_read(&acl_hit_shared_cfg->version);
	if (v1 != v2) {
		v1 = v2;
	}
	cmdline_printf(cl, "ACLHITS: count=%u rule_version=%" PRIu64 " (version %" PRIu64 ")\n", count, rv, v1);
	for (uint32_t i = 0; i < count; i++) {
		cmdline_printf(cl, "%u pkts=%" PRIu64 " bytes=%" PRIu64 "\n", i, pkts[i], bytes[i]);
	}
}

static void cmd_acl6_hits_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!acl6_hit_shared_cfg) {
		cmdline_printf(cl, "ACL6HITS: count=0 rule_version=0 (version 0)\n");
		return;
	}
	uint64_t v1 = rte_atomic64_read(&acl6_hit_shared_cfg->version);
	uint32_t count = acl6_hit_shared_cfg->count;
	uint64_t rv = acl6_hit_shared_cfg->rule_version;
	uint64_t pkts[ACL6_MAX_RULES];
	uint64_t bytes[ACL6_MAX_RULES];
	if (count > ACL6_MAX_RULES) {
		count = ACL6_MAX_RULES;
	}
	memcpy(pkts, acl6_hit_shared_cfg->deny_pkts, sizeof(pkts));
	memcpy(bytes, acl6_hit_shared_cfg->deny_bytes, sizeof(bytes));
	rte_rmb();
	uint64_t v2 = rte_atomic64_read(&acl6_hit_shared_cfg->version);
	if (v1 != v2) {
		v1 = v2;
	}
	cmdline_printf(cl, "ACL6HITS: count=%u rule_version=%" PRIu64 " (version %" PRIu64 ")\n", count, rv, v1);
	for (uint32_t i = 0; i < count; i++) {
		cmdline_printf(cl, "%u pkts=%" PRIu64 " bytes=%" PRIu64 "\n", i, pkts[i], bytes[i]);
	}
}

static int read_shared_rules6(struct acl6_rule *rules, uint32_t *count, uint64_t *version, uint64_t target_version) {
	if (!acl6_shared_cfg || !rules || !count || !version) {
		return -1;
	}
	for (int i = 0; i < 200; i++) {
		uint64_t v1 = rte_atomic64_read(&acl6_shared_cfg->version);
		uint32_t c = acl6_shared_cfg->count;
		if (c > ACL6_MAX_RULES) {
			c = ACL6_MAX_RULES;
		}
		memcpy(rules, acl6_shared_cfg->rules, sizeof(struct acl6_rule) * c);
		uint64_t v2 = rte_atomic64_read(&acl6_shared_cfg->version);
		if (v1 == v2 && (target_version == 0 || v2 == target_version)) {
			*count = c;
			*version = v2;
			return 0;
		}
		rte_delay_us_sleep(1000);
	}
	return -1;
}

static void print_acl6_rule(struct cmdline *cl, const struct acl6_cmd_resp *item) {
	char srcbuf[96];
	char dstbuf[96];
	print_ipv6_cidr(srcbuf, sizeof(srcbuf), &item->rule.src_ip, item->rule.src_depth);
	print_ipv6_cidr(dstbuf, sizeof(dstbuf), &item->rule.dst_ip, item->rule.dst_depth);
	if (item->rule.match_ports) {
		cmdline_printf(cl, "%u %s src=%s dst=%s proto=%u sport=%u-%u dport=%u-%u\n",
			item->rule_index, item->rule.allow ? "allow" : "deny",
			srcbuf, dstbuf, item->rule.proto,
			item->rule.src_port_min, item->rule.src_port_max,
			item->rule.dst_port_min, item->rule.dst_port_max);
	} else {
		cmdline_printf(cl, "%u %s src=%s dst=%s proto=any ports=any\n",
			item->rule_index, item->rule.allow ? "allow" : "deny", srcbuf, dstbuf);
	}
}

static void cmd_acl6_add_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_acl6_add_result *res = parsed_result;
	if (res->src_port_min > res->src_port_max || res->dst_port_min > res->dst_port_max) {
		cmdline_printf(cl, "Invalid port range\n");
		return;
	}
	struct acl6_rule rule;
	memset(&rule, 0, sizeof(rule));
	if (parse_ipaddr_v6(&res->src, &rule.src_ip, &rule.src_depth) != 0 ||
	    parse_ipaddr_v6(&res->dst, &rule.dst_ip, &rule.dst_depth) != 0) {
		cmdline_printf(cl, "Only IPv6 is supported\n");
		return;
	}
	rule.proto = res->proto;
	rule.src_port_min = res->src_port_min;
	rule.src_port_max = res->src_port_max;
	rule.dst_port_min = res->dst_port_min;
	rule.dst_port_max = res->dst_port_max;
	rule.match_ports = (rule.proto != 0 || rule.src_port_max != 0 || rule.dst_port_max != 0);
	rule.allow = strcmp(res->action, "allow") == 0 ? 1 : 0;

	struct acl6_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL6_CMD_ADD;
	cmd->seq = ++cmd_seq;
	cmd->rule = rule;

	struct acl6_cmd_resp *resp = NULL;
	if (send_cmd_and_wait6(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL6 add failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL6 add failed\n");
	} else {
		cmdline_printf(cl, "ACL6 rule added\n");
	}
	rte_free(resp);
}

static void cmd_acl6_del_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_acl6_del_result *res = parsed_result;
	struct acl6_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL6_CMD_DEL;
	cmd->seq = ++cmd_seq;
	cmd->index = res->index;

	struct acl6_cmd_resp *resp = NULL;
	if (send_cmd_and_wait6(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL6 delete failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL6 delete failed\n");
	} else {
		cmdline_printf(cl, "ACL6 rule deleted\n");
	}
	rte_free(resp);
}

static void cmd_acl6_clear_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	struct acl6_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL6_CMD_CLEAR;
	cmd->seq = ++cmd_seq;

	struct acl6_cmd_resp *resp = NULL;
	if (send_cmd_and_wait6(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL6 clear failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL6 clear failed\n");
	} else {
		cmdline_printf(cl, "ACL6 cleared\n");
	}
	rte_free(resp);
}

static void cmd_acl6_list_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	struct acl6_cmd_msg *cmd = rte_zmalloc(NULL, sizeof(*cmd), 0);
	if (!cmd) {
		cmdline_printf(cl, "No memory\n");
		return;
	}
	cmd->type = ACL6_CMD_LIST;
	cmd->seq = ++cmd_seq;

	struct acl6_cmd_resp *resp = NULL;
	if (send_cmd_and_wait6(cmd, &resp) != 0 || !resp) {
		cmdline_printf(cl, "ACL6 list failed\n");
		return;
	}
	if (resp->status != 0) {
		cmdline_printf(cl, "ACL6 list failed (cmd status)\n");
		rte_free(resp);
		return;
	}
	uint64_t target_version = resp->version;
	rte_free(resp);

	struct acl6_rule rules[ACL6_MAX_RULES];
	uint32_t count = 0;
	uint64_t version = 0;
	if (read_shared_rules6(rules, &count, &version, target_version) != 0) {
		if (read_shared_rules6(rules, &count, &version, 0) != 0) {
			cmdline_printf(cl, "ACL6 list failed (shared cfg)\n");
			return;
		}
	}
	cmdline_printf(cl, "ACL6 rules: %u (version %" PRIu64 ")\n", count, version);
	for (uint32_t i = 0; i < count; i++) {
		struct acl6_cmd_resp item;
		memset(&item, 0, sizeof(item));
		item.rule = rules[i];
		item.rule_index = i;
		print_acl6_rule(cl, &item);
	}
}

static void cmd_acl_quit_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	cmdline_quit(cl);
}

static void cmd_session_list_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_session_list_result *res = parsed_result;
	uint32_t limit = res->limit;
	if (limit == 0 || limit > SESSION_MAX_EXPORT) {
		limit = SESSION_MAX_EXPORT;
	}
	if (!session_shared_cfg) {
		cmdline_printf(cl, "SESSIONS: 0 (version 0)\n");
		return;
	}
	uint64_t v1 = rte_atomic64_read(&session_shared_cfg->version);
	uint32_t count = session_shared_cfg->count;
	if (count > limit) {
		count = limit;
	}
	uint64_t now = rte_get_timer_cycles();
	uint64_t hz = rte_get_timer_hz();
	uint64_t v2 = rte_atomic64_read(&session_shared_cfg->version);
	if (v1 != v2) {
		v1 = v2;
	}
	cmdline_printf(cl, "SESSIONS: %u (version %" PRIu64 ")\n", count, v1);
	for (uint32_t i = 0; i < count; i++) {
		const struct session_key *k = &session_shared_cfg->keys[i];
		const struct session_entry *e = &session_shared_cfg->entries[i];
		char srcip[INET_ADDRSTRLEN];
		char dstip[INET_ADDRSTRLEN];
		print_ipv4(srcip, sizeof(srcip), k->src_ip);
		print_ipv4(dstip, sizeof(dstip), k->dst_ip);
		uint64_t last_ms = 0;
		if (e->last_seen_tsc && hz) {
			last_ms = (now - e->last_seen_tsc) * 1000ULL / hz;
		}
		cmdline_printf(cl, "%u proto=%u src=%s:%u dst=%s:%u packets=%" PRIu64 " bytes=%" PRIu64 " last_seen_ms=%" PRIu64 "\n",
			i, k->proto, srcip, k->src_port, dstip, k->dst_port, e->packets, e->bytes, last_ms);
	}
}

static void cmd_session6_list_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_session6_list_result *res = parsed_result;
	uint32_t limit = res->limit;
	if (limit == 0 || limit > SESSION6_MAX_EXPORT) {
		limit = SESSION6_MAX_EXPORT;
	}
	if (!session6_shared_cfg) {
		cmdline_printf(cl, "SESSIONS6: 0 (version 0)\n");
		return;
	}
	uint64_t v1 = rte_atomic64_read(&session6_shared_cfg->version);
	uint32_t count = session6_shared_cfg->count;
	if (count > limit) {
		count = limit;
	}
	uint64_t now = rte_get_timer_cycles();
	uint64_t hz = rte_get_timer_hz();
	uint64_t v2 = rte_atomic64_read(&session6_shared_cfg->version);
	if (v1 != v2) {
		v1 = v2;
	}
	cmdline_printf(cl, "SESSIONS6: %u (version %" PRIu64 ")\n", count, v1);
	for (uint32_t i = 0; i < count; i++) {
		const struct session6_key *k = &session6_shared_cfg->keys[i];
		const struct session_entry *e = &session6_shared_cfg->entries[i];
		char srcip[INET6_ADDRSTRLEN];
		char dstip[INET6_ADDRSTRLEN];
		print_ipv6(srcip, sizeof(srcip), &k->src_ip6);
		print_ipv6(dstip, sizeof(dstip), &k->dst_ip6);
		uint64_t last_ms = 0;
		if (e->last_seen_tsc && hz) {
			last_ms = (now - e->last_seen_tsc) * 1000ULL / hz;
		}
		cmdline_printf(cl, "%u proto=%u src=[%s]:%u dst=[%s]:%u packets=%" PRIu64 " bytes=%" PRIu64 " last_seen_ms=%" PRIu64 "\n",
			i, k->proto, srcip, k->src_port, dstip, k->dst_port, e->packets, e->bytes, last_ms);
	}
}

static void cmd_port_stats_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!portstats_shared_cfg) {
		cmdline_printf(cl, "PORTSTATS: mask=0x0 (version 0)\n");
		return;
	}

	struct portstats_item ports[RTE_MAX_ETHPORTS];
	uint32_t mask = 0;
	uint64_t v1 = rte_atomic64_read(&portstats_shared_cfg->version);
	mask = portstats_shared_cfg->enabled_port_mask;
	memcpy(ports, portstats_shared_cfg->ports, sizeof(ports));
	uint64_t v2 = rte_atomic64_read(&portstats_shared_cfg->version);
	if (v1 != v2) {
		v1 = v2;
		mask = portstats_shared_cfg->enabled_port_mask;
		memcpy(ports, portstats_shared_cfg->ports, sizeof(ports));
	}

	cmdline_printf(cl, "PORTSTATS: mask=0x%x (version %" PRIu64 ")\n", mask, v1);
	for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
		if ((mask & (1u << p)) == 0) {
			continue;
		}
		cmdline_printf(cl, "port=%u rx=%" PRIu64 " tx=%" PRIu64 " dropped=%" PRIu64 " link=%s speed=%u duplex=%s mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
			p, ports[p].rx, ports[p].tx, ports[p].dropped,
			ports[p].link_up ? "up" : "down",
			ports[p].link_speed,
			ports[p].link_duplex ? "full" : "half",
			ports[p].mac[0], ports[p].mac[1], ports[p].mac[2], ports[p].mac[3], ports[p].mac[4], ports[p].mac[5]);
	}
}

static void cmd_deny_list_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_deny_list_result *res = parsed_result;
	uint32_t limit = res->limit;
	if (limit == 0 || limit > DENYLOG_MAX) {
		limit = DENYLOG_MAX;
	}
	if (!denylog_shared_cfg) {
		cmdline_printf(cl, "DENIES: 0 (version 0)\n");
		return;
	}

	struct denylog_entry snapshot[DENYLOG_MAX];
	uint32_t head = 0;
	uint32_t count = 0;
	uint64_t version = 0;
	uint64_t now = rte_get_timer_cycles();
	uint64_t hz = denylog_shared_cfg->tsc_hz ? denylog_shared_cfg->tsc_hz : rte_get_timer_hz();

	rte_spinlock_lock(&denylog_shared_cfg->lock);
	version = rte_atomic64_read(&denylog_shared_cfg->version);
	head = denylog_shared_cfg->head;
	count = denylog_shared_cfg->count;
	if (count > DENYLOG_MAX) {
		count = DENYLOG_MAX;
	}
	memcpy(snapshot, denylog_shared_cfg->entries, sizeof(snapshot));
	rte_spinlock_unlock(&denylog_shared_cfg->lock);

	if (count > limit) {
		count = limit;
	}
	cmdline_printf(cl, "DENIES: %u (version %" PRIu64 ")\n", count, version);
	for (uint32_t i = 0; i < count; i++) {
		uint32_t idx = (head + DENYLOG_MAX - 1 - i) % DENYLOG_MAX;
		const struct denylog_entry *e = &snapshot[idx];
		char srcip[INET_ADDRSTRLEN];
		char dstip[INET_ADDRSTRLEN];
		print_ipv4(srcip, sizeof(srcip), e->src_ip);
		print_ipv4(dstip, sizeof(dstip), e->dst_ip);
		uint64_t age_ms = 0;
		if (e->tsc && hz) {
			age_ms = (now - e->tsc) * 1000ULL / hz;
		}
		cmdline_printf(cl, "%u age_ms=%" PRIu64 " in_port=%u proto=%u src=%s:%u dst=%s:%u rule=%u\n",
			i, age_ms, e->in_port, e->proto, srcip, e->src_port, dstip, e->dst_port, e->rule_index);
	}
}

static void cmd_deny6_list_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_deny6_list_result *res = parsed_result;
	uint32_t limit = res->limit;
	if (limit == 0 || limit > DENYLOG6_MAX) {
		limit = DENYLOG6_MAX;
	}
	if (!denylog6_shared_cfg) {
		cmdline_printf(cl, "DENIES6: 0 (version 0)\n");
		return;
	}

	struct denylog6_entry snapshot[DENYLOG6_MAX];
	uint32_t head = 0;
	uint32_t count = 0;
	uint64_t version = 0;
	uint64_t now = rte_get_timer_cycles();
	uint64_t hz = denylog6_shared_cfg->tsc_hz ? denylog6_shared_cfg->tsc_hz : rte_get_timer_hz();

	rte_spinlock_lock(&denylog6_shared_cfg->lock);
	version = rte_atomic64_read(&denylog6_shared_cfg->version);
	head = denylog6_shared_cfg->head;
	count = denylog6_shared_cfg->count;
	if (count > DENYLOG6_MAX) {
		count = DENYLOG6_MAX;
	}
	memcpy(snapshot, denylog6_shared_cfg->entries, sizeof(snapshot));
	rte_spinlock_unlock(&denylog6_shared_cfg->lock);

	if (count > limit) {
		count = limit;
	}
	cmdline_printf(cl, "DENIES6: %u (version %" PRIu64 ")\n", count, version);
	for (uint32_t i = 0; i < count; i++) {
		uint32_t idx = (head + DENYLOG6_MAX - 1 - i) % DENYLOG6_MAX;
		const struct denylog6_entry *e = &snapshot[idx];
		char srcip[INET6_ADDRSTRLEN];
		char dstip[INET6_ADDRSTRLEN];
		print_ipv6(srcip, sizeof(srcip), &e->src_ip6);
		print_ipv6(dstip, sizeof(dstip), &e->dst_ip6);
		uint64_t age_ms = 0;
		if (e->tsc && hz) {
			age_ms = (now - e->tsc) * 1000ULL / hz;
		}
		cmdline_printf(cl, "%u age_ms=%" PRIu64 " in_port=%u proto=%u src=[%s]:%u dst=[%s]:%u rule=%u\n",
			i, age_ms, e->in_port, e->proto, srcip, e->src_port, dstip, e->dst_port, e->rule_index);
	}
}

static int parse_ipv6_cidr_str(const char *s, struct rte_ipv6_addr *ip, uint8_t *depth) {
	if (!s || !ip || !depth) {
		return -1;
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", s);
	char *slash = strchr(buf, '/');
	if (!slash) {
		return -1;
	}
	*slash = '\0';
	const char *ip_str = buf;
	const char *d_str = slash + 1;
	unsigned long d = strtoul(d_str, NULL, 10);
	if (d > 128) {
		return -1;
	}
	struct in6_addr a6;
	if (inet_pton(AF_INET6, ip_str, &a6) != 1) {
		return -1;
	}
	memcpy(ip, &a6, sizeof(*ip));
	*depth = (uint8_t)d;
	return 0;
}

static int parse_ipv6_addr_str(const char *s, struct rte_ipv6_addr *ip) {
	if (!s || !ip) {
		return -1;
	}
	struct in6_addr a6;
	if (inet_pton(AF_INET6, s, &a6) != 1) {
		return -1;
	}
	memcpy(ip, &a6, sizeof(*ip));
	return 0;
}

static void cmd_ifcfg6_show_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!route6_shared_cfg) {
		cmdline_printf(cl, "IFCFG6: 0 (version 0)\n");
		return;
	}
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	uint32_t cnt = 0;
	for (uint32_t p = 0; p < IFACE6_MAX_PORTS; p++) {
		if (route6_shared_cfg->ifcfg6s[p].configured) {
			cnt++;
		}
	}
	cmdline_printf(cl, "IFCFG6: %u (version %" PRIu64 ")\n", cnt, v);
	for (uint32_t p = 0; p < IFACE6_MAX_PORTS; p++) {
		const struct ifcfg6_item *it = &route6_shared_cfg->ifcfg6s[p];
		if (!it->configured) {
			continue;
		}
		char ipbuf[INET6_ADDRSTRLEN];
		print_ipv6(ipbuf, sizeof(ipbuf), &it->ip);
		cmdline_printf(cl, "port=%u ip=%s/%u\n", p, ipbuf, it->depth);
	}
}

static void cmd_ifcfg6_set_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_ifcfg6_set_result *res = parsed_result;
	if (!route6_shared_cfg) {
		cmdline_printf(cl, "ifcfg6 set failed\n");
		return;
	}
	if (res->port >= IFACE6_MAX_PORTS) {
		cmdline_printf(cl, "ifcfg6 set failed\n");
		return;
	}
	struct rte_ipv6_addr ip;
	uint8_t depth = 0;
	if (parse_ipv6_cidr_str(res->cidr, &ip, &depth) != 0) {
		cmdline_printf(cl, "ifcfg6 set failed\n");
		return;
	}
	route6_shared_cfg->ifcfg6s[res->port].ip = ip;
	route6_shared_cfg->ifcfg6s[res->port].depth = depth;
	route6_shared_cfg->ifcfg6s[res->port].configured = 1;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	rte_atomic64_set(&route6_shared_cfg->version, v + 1);
	cmdline_printf(cl, "ifcfg6 set ok\n");
}

static void cmd_ifcfg6_clear_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_ifcfg6_clear_result *res = parsed_result;
	if (!route6_shared_cfg) {
		cmdline_printf(cl, "ifcfg6 clear failed\n");
		return;
	}
	if (res->port >= IFACE6_MAX_PORTS) {
		cmdline_printf(cl, "ifcfg6 clear failed\n");
		return;
	}
	memset(&route6_shared_cfg->ifcfg6s[res->port], 0, sizeof(route6_shared_cfg->ifcfg6s[res->port]));
	rte_wmb();
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	rte_atomic64_set(&route6_shared_cfg->version, v + 1);
	cmdline_printf(cl, "ifcfg6 clear ok\n");
}

static void cmd_route6_list_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!route6_shared_cfg) {
		cmdline_printf(cl, "ROUTE6: 0 (version 0)\n");
		return;
	}
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	uint32_t c = route6_shared_cfg->route_count;
	if (c > ROUTE6_MAX) {
		c = ROUTE6_MAX;
	}
	cmdline_printf(cl, "ROUTE6: %u (version %" PRIu64 ")\n", c, v);
	for (uint32_t i = 0; i < c; i++) {
		const struct route6_item *r = &route6_shared_cfg->routes[i];
		char dstbuf[INET6_ADDRSTRLEN];
		char nhbuf[INET6_ADDRSTRLEN];
		print_ipv6(dstbuf, sizeof(dstbuf), &r->dst);
		print_ipv6(nhbuf, sizeof(nhbuf), &r->next_hop);
		cmdline_printf(cl, "%u dst=%s/%u nh=%s port=%u\n", i, dstbuf, r->depth, nhbuf, r->out_port);
	}
}

static void cmd_route6_clear_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!route6_shared_cfg) {
		cmdline_printf(cl, "route6 clear failed\n");
		return;
	}
	route6_shared_cfg->route_count = 0;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	rte_atomic64_set(&route6_shared_cfg->version, v + 1);
	cmdline_printf(cl, "route6 clear ok\n");
}

static void cmd_route6_del_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_route6_del_result *res = parsed_result;
	if (!route6_shared_cfg) {
		cmdline_printf(cl, "route6 del failed\n");
		return;
	}
	uint32_t c = route6_shared_cfg->route_count;
	if (c > ROUTE6_MAX) {
		c = ROUTE6_MAX;
	}
	if (res->index >= c) {
		cmdline_printf(cl, "route6 del failed\n");
		return;
	}
	if (res->index + 1 < c) {
		memmove(&route6_shared_cfg->routes[res->index], &route6_shared_cfg->routes[res->index + 1],
			sizeof(struct route6_item) * (c - res->index - 1));
	}
	route6_shared_cfg->route_count = c - 1;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	rte_atomic64_set(&route6_shared_cfg->version, v + 1);
	cmdline_printf(cl, "route6 del ok\n");
}

static void cmd_route6_add_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_route6_add_result *res = parsed_result;
	if (!route6_shared_cfg) {
		cmdline_printf(cl, "route6 add failed\n");
		return;
	}
	if (res->port >= IFACE6_MAX_PORTS) {
		cmdline_printf(cl, "route6 add failed\n");
		return;
	}
	uint32_t c = route6_shared_cfg->route_count;
	if (c >= ROUTE6_MAX) {
		cmdline_printf(cl, "route6 add failed\n");
		return;
	}
	struct rte_ipv6_addr dst;
	uint8_t depth = 0;
	if (parse_ipv6_cidr_str(res->dst, &dst, &depth) != 0) {
		cmdline_printf(cl, "route6 add failed\n");
		return;
	}
	struct rte_ipv6_addr nh = RTE_IPV6_ADDR_UNSPEC;
	if (strcmp(res->nh, "::") != 0) {
		if (parse_ipv6_addr_str(res->nh, &nh) != 0) {
			cmdline_printf(cl, "route6 add failed\n");
			return;
		}
	}
	struct route6_item *it = &route6_shared_cfg->routes[c];
	memset(it, 0, sizeof(*it));
	it->dst = dst;
	it->depth = depth;
	it->next_hop = nh;
	it->out_port = (uint16_t)res->port;
	route6_shared_cfg->route_count = c + 1;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	rte_atomic64_set(&route6_shared_cfg->version, v + 1);
	cmdline_printf(cl, "route6 add ok\n");
}

static void cmd_attack_show_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!attack_shared_cfg) {
		cmdline_printf(cl, "ATTACK: mitigation=0 scan_ports_sec=0 ban_sec=0 syn_pps=0 udp_pps=0 scan_events=0 scan_banned=0 top4=-/0 top6=-/0 (version 0)\n");
		return;
	}
	uint64_t v1 = rte_atomic64_read(&attack_shared_cfg->version);
	uint32_t scan_ports = attack_shared_cfg->scan_ports_per_sec;
	uint32_t ban_sec = attack_shared_cfg->ban_seconds;
	uint32_t syn_pps = attack_shared_cfg->syn_pps;
	uint32_t udp_pps = attack_shared_cfg->udp_pps;
	uint32_t scan_events = attack_shared_cfg->scan_events;
	uint32_t scan_banned = attack_shared_cfg->scan_banned;
	uint8_t mit = attack_shared_cfg->mitigation_enabled;
	uint32_t top4_ip = attack_shared_cfg->top_scan4_ip;
	uint32_t top4_ports = attack_shared_cfg->top_scan4_ports;
	struct rte_ipv6_addr top6_ip = attack_shared_cfg->top_scan6_ip;
	uint32_t top6_ports = attack_shared_cfg->top_scan6_ports;
	rte_rmb();
	uint64_t v2 = rte_atomic64_read(&attack_shared_cfg->version);
	if (v1 != v2) {
		v1 = v2;
	}
	char top4buf[INET_ADDRSTRLEN];
	if (top4_ip) {
		print_ipv4(top4buf, sizeof(top4buf), top4_ip);
	} else {
		snprintf(top4buf, sizeof(top4buf), "-");
	}
	char top6buf[INET6_ADDRSTRLEN];
	const struct rte_ipv6_addr unspec = RTE_IPV6_ADDR_UNSPEC;
	if (!rte_ipv6_addr_eq(&top6_ip, &unspec)) {
		print_ipv6(top6buf, sizeof(top6buf), &top6_ip);
	} else {
		snprintf(top6buf, sizeof(top6buf), "-");
	}
	cmdline_printf(cl,
		"ATTACK: mitigation=%u scan_ports_sec=%u ban_sec=%u syn_pps=%u udp_pps=%u scan_events=%u scan_banned=%u top4=%s/%u top6=%s/%u (version %" PRIu64 ")\n",
		mit ? 1u : 0u, scan_ports, ban_sec, syn_pps, udp_pps, scan_events, scan_banned,
		top4buf, top4_ports, top6buf, top6_ports, v1);
}

static void cmd_attack_set_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_attack_set_result *res = parsed_result;
	if (!attack_shared_cfg) {
		cmdline_printf(cl, "attack set failed\n");
		return;
	}
	if (res->mitigation > 1) {
		cmdline_printf(cl, "attack set failed\n");
		return;
	}
	if (res->scan_ports_sec == 0 || res->ban_sec == 0) {
		cmdline_printf(cl, "attack set failed\n");
		return;
	}
	attack_shared_cfg->mitigation_enabled = (uint8_t)res->mitigation;
	attack_shared_cfg->scan_ports_per_sec = res->scan_ports_sec;
	attack_shared_cfg->ban_seconds = res->ban_sec;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&attack_shared_cfg->version);
	rte_atomic64_set(&attack_shared_cfg->version, v + 1);
	cmdline_printf(cl, "attack set ok\n");
}

static void cmd_ddos_show_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	if (!rlim_shared_cfg) {
		cmdline_printf(cl, "DDOS: syn_pps=0 syn_burst=0 udp_pps=0 udp_burst=0 (version 0)\n");
		return;
	}
	uint64_t v1 = rte_atomic64_read(&rlim_shared_cfg->version);
	uint32_t syn_pps = rlim_shared_cfg->syn_pps;
	uint32_t syn_burst = rlim_shared_cfg->syn_burst;
	uint32_t udp_pps = rlim_shared_cfg->udp_pps;
	uint32_t udp_burst = rlim_shared_cfg->udp_burst;
	rte_rmb();
	uint64_t v2 = rte_atomic64_read(&rlim_shared_cfg->version);
	if (v2 != v1) {
		v1 = v2;
		syn_pps = rlim_shared_cfg->syn_pps;
		syn_burst = rlim_shared_cfg->syn_burst;
		udp_pps = rlim_shared_cfg->udp_pps;
		udp_burst = rlim_shared_cfg->udp_burst;
	}
	cmdline_printf(cl, "DDOS: syn_pps=%u syn_burst=%u udp_pps=%u udp_burst=%u (version %" PRIu64 ")\n",
		syn_pps, syn_burst, udp_pps, udp_burst, v1);
}

static void cmd_ddos_set_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)data;
	struct cmd_ddos_set_result *res = parsed_result;
	if (!rlim_shared_cfg) {
		cmdline_printf(cl, "DDOS set failed\n");
		return;
	}
	if ((res->syn_pps != 0 && res->syn_burst == 0) || (res->udp_pps != 0 && res->udp_burst == 0)) {
		cmdline_printf(cl, "DDOS set failed\n");
		return;
	}
	rlim_shared_cfg->syn_pps = res->syn_pps;
	rlim_shared_cfg->syn_burst = res->syn_burst;
	rlim_shared_cfg->udp_pps = res->udp_pps;
	rlim_shared_cfg->udp_burst = res->udp_burst;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&rlim_shared_cfg->version);
	rte_atomic64_set(&rlim_shared_cfg->version, v + 1);
	cmdline_printf(cl, "DDOS set ok\n");
}

cmdline_parse_token_string_t cmd_acl_add_acl =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_add_result, acl, "acl");
cmdline_parse_token_string_t cmd_acl_add_add =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_add_result, add, "add");
cmdline_parse_token_string_t cmd_acl_add_action =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_add_result, action, "allow#deny");
cmdline_parse_token_ipaddr_t cmd_acl_add_src =
	TOKEN_IPV4NET_INITIALIZER(struct cmd_acl_add_result, src);
cmdline_parse_token_ipaddr_t cmd_acl_add_dst =
	TOKEN_IPV4NET_INITIALIZER(struct cmd_acl_add_result, dst);
cmdline_parse_token_num_t cmd_acl_add_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_acl_add_result, proto, RTE_UINT8);
cmdline_parse_token_num_t cmd_acl_add_src_port_min =
	TOKEN_NUM_INITIALIZER(struct cmd_acl_add_result, src_port_min, RTE_UINT16);
cmdline_parse_token_num_t cmd_acl_add_src_port_max =
	TOKEN_NUM_INITIALIZER(struct cmd_acl_add_result, src_port_max, RTE_UINT16);
cmdline_parse_token_num_t cmd_acl_add_dst_port_min =
	TOKEN_NUM_INITIALIZER(struct cmd_acl_add_result, dst_port_min, RTE_UINT16);
cmdline_parse_token_num_t cmd_acl_add_dst_port_max =
	TOKEN_NUM_INITIALIZER(struct cmd_acl_add_result, dst_port_max, RTE_UINT16);

cmdline_parse_inst_t cmd_acl_add = {
	.f = cmd_acl_add_parsed,
	.data = NULL,
	.help_str = "acl add allow|deny src_ip/prefix dst_ip/prefix proto sp_min sp_max dp_min dp_max",
	.tokens = {
		(void *)&cmd_acl_add_acl,
		(void *)&cmd_acl_add_add,
		(void *)&cmd_acl_add_action,
		(void *)&cmd_acl_add_src,
		(void *)&cmd_acl_add_dst,
		(void *)&cmd_acl_add_proto,
		(void *)&cmd_acl_add_src_port_min,
		(void *)&cmd_acl_add_src_port_max,
		(void *)&cmd_acl_add_dst_port_min,
		(void *)&cmd_acl_add_dst_port_max,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl_del_acl =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_del_result, acl, "acl");
cmdline_parse_token_string_t cmd_acl_del_del =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_del_result, del, "del");
cmdline_parse_token_num_t cmd_acl_del_index =
	TOKEN_NUM_INITIALIZER(struct cmd_acl_del_result, index, RTE_UINT32);

cmdline_parse_inst_t cmd_acl_del = {
	.f = cmd_acl_del_parsed,
	.data = NULL,
	.help_str = "acl del <index>",
	.tokens = {
		(void *)&cmd_acl_del_acl,
		(void *)&cmd_acl_del_del,
		(void *)&cmd_acl_del_index,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl_clear_acl =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_clear_result, acl, "acl");
cmdline_parse_token_string_t cmd_acl_clear_clear =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_clear_result, clear, "clear");

cmdline_parse_inst_t cmd_acl_clear = {
	.f = cmd_acl_clear_parsed,
	.data = NULL,
	.help_str = "acl clear",
	.tokens = {
		(void *)&cmd_acl_clear_acl,
		(void *)&cmd_acl_clear_clear,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl_list_acl =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_list_result, acl, "acl");
cmdline_parse_token_string_t cmd_acl_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_list_result, list, "list");

cmdline_parse_inst_t cmd_acl_list = {
	.f = cmd_acl_list_parsed,
	.data = NULL,
	.help_str = "acl list",
	.tokens = {
		(void *)&cmd_acl_list_acl,
		(void *)&cmd_acl_list_list,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl_hits_acl =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_hits_result, acl, "acl");
cmdline_parse_token_string_t cmd_acl_hits_hits =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_hits_result, hits, "hits");

cmdline_parse_inst_t cmd_acl_hits = {
	.f = cmd_acl_hits_parsed,
	.data = NULL,
	.help_str = "acl hits",
	.tokens = {
		(void *)&cmd_acl_hits_acl,
		(void *)&cmd_acl_hits_hits,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl6_hits_acl6 =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_hits_result, acl6, "acl6");
cmdline_parse_token_string_t cmd_acl6_hits_hits =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_hits_result, hits, "hits");

cmdline_parse_inst_t cmd_acl6_hits = {
	.f = cmd_acl6_hits_parsed,
	.data = NULL,
	.help_str = "acl6 hits",
	.tokens = {
		(void *)&cmd_acl6_hits_acl6,
		(void *)&cmd_acl6_hits_hits,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl6_add_acl6 =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_add_result, acl6, "acl6");
cmdline_parse_token_string_t cmd_acl6_add_add =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_add_result, add, "add");
cmdline_parse_token_string_t cmd_acl6_add_action =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_add_result, action, "allow#deny");
cmdline_parse_token_ipaddr_t cmd_acl6_add_src =
	TOKEN_IPNET_INITIALIZER(struct cmd_acl6_add_result, src);
cmdline_parse_token_ipaddr_t cmd_acl6_add_dst =
	TOKEN_IPNET_INITIALIZER(struct cmd_acl6_add_result, dst);
cmdline_parse_token_num_t cmd_acl6_add_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_acl6_add_result, proto, RTE_UINT8);
cmdline_parse_token_num_t cmd_acl6_add_src_port_min =
	TOKEN_NUM_INITIALIZER(struct cmd_acl6_add_result, src_port_min, RTE_UINT16);
cmdline_parse_token_num_t cmd_acl6_add_src_port_max =
	TOKEN_NUM_INITIALIZER(struct cmd_acl6_add_result, src_port_max, RTE_UINT16);
cmdline_parse_token_num_t cmd_acl6_add_dst_port_min =
	TOKEN_NUM_INITIALIZER(struct cmd_acl6_add_result, dst_port_min, RTE_UINT16);
cmdline_parse_token_num_t cmd_acl6_add_dst_port_max =
	TOKEN_NUM_INITIALIZER(struct cmd_acl6_add_result, dst_port_max, RTE_UINT16);

cmdline_parse_inst_t cmd_acl6_add = {
	.f = cmd_acl6_add_parsed,
	.data = NULL,
	.help_str = "acl6 add allow|deny src_ip6/prefix dst_ip6/prefix proto sp_min sp_max dp_min dp_max",
	.tokens = {
		(void *)&cmd_acl6_add_acl6,
		(void *)&cmd_acl6_add_add,
		(void *)&cmd_acl6_add_action,
		(void *)&cmd_acl6_add_src,
		(void *)&cmd_acl6_add_dst,
		(void *)&cmd_acl6_add_proto,
		(void *)&cmd_acl6_add_src_port_min,
		(void *)&cmd_acl6_add_src_port_max,
		(void *)&cmd_acl6_add_dst_port_min,
		(void *)&cmd_acl6_add_dst_port_max,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl6_del_acl6 =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_del_result, acl6, "acl6");
cmdline_parse_token_string_t cmd_acl6_del_del =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_del_result, del, "del");
cmdline_parse_token_num_t cmd_acl6_del_index =
	TOKEN_NUM_INITIALIZER(struct cmd_acl6_del_result, index, RTE_UINT32);

cmdline_parse_inst_t cmd_acl6_del = {
	.f = cmd_acl6_del_parsed,
	.data = NULL,
	.help_str = "acl6 del <index>",
	.tokens = {
		(void *)&cmd_acl6_del_acl6,
		(void *)&cmd_acl6_del_del,
		(void *)&cmd_acl6_del_index,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl6_clear_acl6 =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_clear_result, acl6, "acl6");
cmdline_parse_token_string_t cmd_acl6_clear_clear =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_clear_result, clear, "clear");

cmdline_parse_inst_t cmd_acl6_clear = {
	.f = cmd_acl6_clear_parsed,
	.data = NULL,
	.help_str = "acl6 clear",
	.tokens = {
		(void *)&cmd_acl6_clear_acl6,
		(void *)&cmd_acl6_clear_clear,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl6_list_acl6 =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_list_result, acl6, "acl6");
cmdline_parse_token_string_t cmd_acl6_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_acl6_list_result, list, "list");

cmdline_parse_inst_t cmd_acl6_list = {
	.f = cmd_acl6_list_parsed,
	.data = NULL,
	.help_str = "acl6 list",
	.tokens = {
		(void *)&cmd_acl6_list_acl6,
		(void *)&cmd_acl6_list_list,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_acl_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_acl_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_acl_quit = {
	.f = cmd_acl_quit_parsed,
	.data = NULL,
	.help_str = "quit",
	.tokens = {
		(void *)&cmd_acl_quit_quit,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_session_list_session =
	TOKEN_STRING_INITIALIZER(struct cmd_session_list_result, session, "session");
cmdline_parse_token_string_t cmd_session_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_session_list_result, list, "list");
cmdline_parse_token_num_t cmd_session_list_limit =
	TOKEN_NUM_INITIALIZER(struct cmd_session_list_result, limit, RTE_UINT32);

cmdline_parse_inst_t cmd_session_list = {
	.f = cmd_session_list_parsed,
	.data = NULL,
	.help_str = "session list <limit>",
	.tokens = {
		(void *)&cmd_session_list_session,
		(void *)&cmd_session_list_list,
		(void *)&cmd_session_list_limit,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_session6_list_session6 =
	TOKEN_STRING_INITIALIZER(struct cmd_session6_list_result, session6, "session6");
cmdline_parse_token_string_t cmd_session6_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_session6_list_result, list, "list");
cmdline_parse_token_num_t cmd_session6_list_limit =
	TOKEN_NUM_INITIALIZER(struct cmd_session6_list_result, limit, RTE_UINT32);

cmdline_parse_inst_t cmd_session6_list = {
	.f = cmd_session6_list_parsed,
	.data = NULL,
	.help_str = "session6 list <limit>",
	.tokens = {
		(void *)&cmd_session6_list_session6,
		(void *)&cmd_session6_list_list,
		(void *)&cmd_session6_list_limit,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_port_stats_port =
	TOKEN_STRING_INITIALIZER(struct cmd_port_stats_result, port, "port");
cmdline_parse_token_string_t cmd_port_stats_stats =
	TOKEN_STRING_INITIALIZER(struct cmd_port_stats_result, stats, "stats");

cmdline_parse_inst_t cmd_port_stats = {
	.f = cmd_port_stats_parsed,
	.data = NULL,
	.help_str = "port stats",
	.tokens = {
		(void *)&cmd_port_stats_port,
		(void *)&cmd_port_stats_stats,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_deny_list_deny =
	TOKEN_STRING_INITIALIZER(struct cmd_deny_list_result, deny, "deny");
cmdline_parse_token_string_t cmd_deny_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_deny_list_result, list, "list");
cmdline_parse_token_num_t cmd_deny_list_limit =
	TOKEN_NUM_INITIALIZER(struct cmd_deny_list_result, limit, RTE_UINT32);

cmdline_parse_inst_t cmd_deny_list = {
	.f = cmd_deny_list_parsed,
	.data = NULL,
	.help_str = "deny list <limit>",
	.tokens = {
		(void *)&cmd_deny_list_deny,
		(void *)&cmd_deny_list_list,
		(void *)&cmd_deny_list_limit,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_deny6_list_deny6 =
	TOKEN_STRING_INITIALIZER(struct cmd_deny6_list_result, deny6, "deny6");
cmdline_parse_token_string_t cmd_deny6_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_deny6_list_result, list, "list");
cmdline_parse_token_num_t cmd_deny6_list_limit =
	TOKEN_NUM_INITIALIZER(struct cmd_deny6_list_result, limit, RTE_UINT32);

cmdline_parse_inst_t cmd_deny6_list = {
	.f = cmd_deny6_list_parsed,
	.data = NULL,
	.help_str = "deny6 list <limit>",
	.tokens = {
		(void *)&cmd_deny6_list_deny6,
		(void *)&cmd_deny6_list_list,
		(void *)&cmd_deny6_list_limit,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_attack_show_attack =
	TOKEN_STRING_INITIALIZER(struct cmd_attack_show_result, attack, "attack");
cmdline_parse_token_string_t cmd_attack_show_show =
	TOKEN_STRING_INITIALIZER(struct cmd_attack_show_result, show, "show");

cmdline_parse_inst_t cmd_attack_show = {
	.f = cmd_attack_show_parsed,
	.data = NULL,
	.help_str = "attack show",
	.tokens = {
		(void *)&cmd_attack_show_attack,
		(void *)&cmd_attack_show_show,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_attack_set_attack =
	TOKEN_STRING_INITIALIZER(struct cmd_attack_set_result, attack, "attack");
cmdline_parse_token_string_t cmd_attack_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_attack_set_result, set, "set");
cmdline_parse_token_num_t cmd_attack_set_mitigation =
	TOKEN_NUM_INITIALIZER(struct cmd_attack_set_result, mitigation, RTE_UINT32);
cmdline_parse_token_num_t cmd_attack_set_scan_ports_sec =
	TOKEN_NUM_INITIALIZER(struct cmd_attack_set_result, scan_ports_sec, RTE_UINT32);
cmdline_parse_token_num_t cmd_attack_set_ban_sec =
	TOKEN_NUM_INITIALIZER(struct cmd_attack_set_result, ban_sec, RTE_UINT32);

cmdline_parse_inst_t cmd_attack_set = {
	.f = cmd_attack_set_parsed,
	.data = NULL,
	.help_str = "attack set <mitigation 0|1> <scan_ports_sec> <ban_sec>",
	.tokens = {
		(void *)&cmd_attack_set_attack,
		(void *)&cmd_attack_set_set,
		(void *)&cmd_attack_set_mitigation,
		(void *)&cmd_attack_set_scan_ports_sec,
		(void *)&cmd_attack_set_ban_sec,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_ddos_show_ddos =
	TOKEN_STRING_INITIALIZER(struct cmd_ddos_show_result, ddos, "ddos");
cmdline_parse_token_string_t cmd_ddos_show_show =
	TOKEN_STRING_INITIALIZER(struct cmd_ddos_show_result, show, "show");

cmdline_parse_inst_t cmd_ddos_show = {
	.f = cmd_ddos_show_parsed,
	.data = NULL,
	.help_str = "ddos show",
	.tokens = {
		(void *)&cmd_ddos_show_ddos,
		(void *)&cmd_ddos_show_show,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_ddos_set_ddos =
	TOKEN_STRING_INITIALIZER(struct cmd_ddos_set_result, ddos, "ddos");
cmdline_parse_token_string_t cmd_ddos_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_ddos_set_result, set, "set");
cmdline_parse_token_num_t cmd_ddos_set_syn_pps =
	TOKEN_NUM_INITIALIZER(struct cmd_ddos_set_result, syn_pps, RTE_UINT32);
cmdline_parse_token_num_t cmd_ddos_set_syn_burst =
	TOKEN_NUM_INITIALIZER(struct cmd_ddos_set_result, syn_burst, RTE_UINT32);
cmdline_parse_token_num_t cmd_ddos_set_udp_pps =
	TOKEN_NUM_INITIALIZER(struct cmd_ddos_set_result, udp_pps, RTE_UINT32);
cmdline_parse_token_num_t cmd_ddos_set_udp_burst =
	TOKEN_NUM_INITIALIZER(struct cmd_ddos_set_result, udp_burst, RTE_UINT32);

cmdline_parse_inst_t cmd_ddos_set = {
	.f = cmd_ddos_set_parsed,
	.data = NULL,
	.help_str = "ddos set <syn_pps> <syn_burst> <udp_pps> <udp_burst>",
	.tokens = {
		(void *)&cmd_ddos_set_ddos,
		(void *)&cmd_ddos_set_set,
		(void *)&cmd_ddos_set_syn_pps,
		(void *)&cmd_ddos_set_syn_burst,
		(void *)&cmd_ddos_set_udp_pps,
		(void *)&cmd_ddos_set_udp_burst,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_ifcfg6_show_ifcfg6 =
	TOKEN_STRING_INITIALIZER(struct cmd_ifcfg6_show_result, ifcfg6, "ifcfg6");
cmdline_parse_token_string_t cmd_ifcfg6_show_show =
	TOKEN_STRING_INITIALIZER(struct cmd_ifcfg6_show_result, show, "show");

cmdline_parse_inst_t cmd_ifcfg6_show = {
	.f = cmd_ifcfg6_show_parsed,
	.data = NULL,
	.help_str = "ifcfg6 show",
	.tokens = {
		(void *)&cmd_ifcfg6_show_ifcfg6,
		(void *)&cmd_ifcfg6_show_show,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_ifcfg6_set_ifcfg6 =
	TOKEN_STRING_INITIALIZER(struct cmd_ifcfg6_set_result, ifcfg6, "ifcfg6");
cmdline_parse_token_string_t cmd_ifcfg6_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_ifcfg6_set_result, set, "set");
cmdline_parse_token_num_t cmd_ifcfg6_set_port =
	TOKEN_NUM_INITIALIZER(struct cmd_ifcfg6_set_result, port, RTE_UINT32);
cmdline_parse_token_string_t cmd_ifcfg6_set_cidr =
	TOKEN_STRING_INITIALIZER(struct cmd_ifcfg6_set_result, cidr, NULL);

cmdline_parse_inst_t cmd_ifcfg6_set = {
	.f = cmd_ifcfg6_set_parsed,
	.data = NULL,
	.help_str = "ifcfg6 set <port> <ip6/prefix>",
	.tokens = {
		(void *)&cmd_ifcfg6_set_ifcfg6,
		(void *)&cmd_ifcfg6_set_set,
		(void *)&cmd_ifcfg6_set_port,
		(void *)&cmd_ifcfg6_set_cidr,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_ifcfg6_clear_ifcfg6 =
	TOKEN_STRING_INITIALIZER(struct cmd_ifcfg6_clear_result, ifcfg6, "ifcfg6");
cmdline_parse_token_string_t cmd_ifcfg6_clear_clear =
	TOKEN_STRING_INITIALIZER(struct cmd_ifcfg6_clear_result, clear, "clear");
cmdline_parse_token_num_t cmd_ifcfg6_clear_port =
	TOKEN_NUM_INITIALIZER(struct cmd_ifcfg6_clear_result, port, RTE_UINT32);

cmdline_parse_inst_t cmd_ifcfg6_clear = {
	.f = cmd_ifcfg6_clear_parsed,
	.data = NULL,
	.help_str = "ifcfg6 clear <port>",
	.tokens = {
		(void *)&cmd_ifcfg6_clear_ifcfg6,
		(void *)&cmd_ifcfg6_clear_clear,
		(void *)&cmd_ifcfg6_clear_port,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_route6_list_route6 =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_list_result, route6, "route6");
cmdline_parse_token_string_t cmd_route6_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_list_result, list, "list");

cmdline_parse_inst_t cmd_route6_list = {
	.f = cmd_route6_list_parsed,
	.data = NULL,
	.help_str = "route6 list",
	.tokens = {
		(void *)&cmd_route6_list_route6,
		(void *)&cmd_route6_list_list,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_route6_clear_route6 =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_clear_result, route6, "route6");
cmdline_parse_token_string_t cmd_route6_clear_clear =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_clear_result, clear, "clear");

cmdline_parse_inst_t cmd_route6_clear = {
	.f = cmd_route6_clear_parsed,
	.data = NULL,
	.help_str = "route6 clear",
	.tokens = {
		(void *)&cmd_route6_clear_route6,
		(void *)&cmd_route6_clear_clear,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_route6_del_route6 =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_del_result, route6, "route6");
cmdline_parse_token_string_t cmd_route6_del_del =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_del_result, del, "del");
cmdline_parse_token_num_t cmd_route6_del_index =
	TOKEN_NUM_INITIALIZER(struct cmd_route6_del_result, index, RTE_UINT32);

cmdline_parse_inst_t cmd_route6_del = {
	.f = cmd_route6_del_parsed,
	.data = NULL,
	.help_str = "route6 del <index>",
	.tokens = {
		(void *)&cmd_route6_del_route6,
		(void *)&cmd_route6_del_del,
		(void *)&cmd_route6_del_index,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_route6_add_route6 =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_add_result, route6, "route6");
cmdline_parse_token_string_t cmd_route6_add_add =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_add_result, add, "add");
cmdline_parse_token_string_t cmd_route6_add_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_add_result, dst, NULL);
cmdline_parse_token_string_t cmd_route6_add_nh =
	TOKEN_STRING_INITIALIZER(struct cmd_route6_add_result, nh, NULL);
cmdline_parse_token_num_t cmd_route6_add_port =
	TOKEN_NUM_INITIALIZER(struct cmd_route6_add_result, port, RTE_UINT32);

cmdline_parse_inst_t cmd_route6_add = {
	.f = cmd_route6_add_parsed,
	.data = NULL,
	.help_str = "route6 add <dst6/prefix> <nexthop6|::> <port>",
	.tokens = {
		(void *)&cmd_route6_add_route6,
		(void *)&cmd_route6_add_add,
		(void *)&cmd_route6_add_dst,
		(void *)&cmd_route6_add_nh,
		(void *)&cmd_route6_add_port,
		NULL,
	},
};

cmdline_parse_ctx_t acl_cmdline_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_acl_add,
	(cmdline_parse_inst_t *)&cmd_acl_del,
	(cmdline_parse_inst_t *)&cmd_acl_clear,
	(cmdline_parse_inst_t *)&cmd_acl_list,
	(cmdline_parse_inst_t *)&cmd_acl_hits,
	(cmdline_parse_inst_t *)&cmd_acl6_add,
	(cmdline_parse_inst_t *)&cmd_acl6_del,
	(cmdline_parse_inst_t *)&cmd_acl6_clear,
	(cmdline_parse_inst_t *)&cmd_acl6_list,
	(cmdline_parse_inst_t *)&cmd_acl6_hits,
	(cmdline_parse_inst_t *)&cmd_session_list,
	(cmdline_parse_inst_t *)&cmd_session6_list,
	(cmdline_parse_inst_t *)&cmd_port_stats,
	(cmdline_parse_inst_t *)&cmd_deny_list,
	(cmdline_parse_inst_t *)&cmd_deny6_list,
	(cmdline_parse_inst_t *)&cmd_ifcfg6_show,
	(cmdline_parse_inst_t *)&cmd_ifcfg6_set,
	(cmdline_parse_inst_t *)&cmd_ifcfg6_clear,
	(cmdline_parse_inst_t *)&cmd_route6_list,
	(cmdline_parse_inst_t *)&cmd_route6_add,
	(cmdline_parse_inst_t *)&cmd_route6_del,
	(cmdline_parse_inst_t *)&cmd_route6_clear,
	(cmdline_parse_inst_t *)&cmd_attack_show,
	(cmdline_parse_inst_t *)&cmd_attack_set,
	(cmdline_parse_inst_t *)&cmd_ddos_show,
	(cmdline_parse_inst_t *)&cmd_ddos_set,
	(cmdline_parse_inst_t *)&cmd_acl_quit,
	NULL,
};

static int parse_cli_args(int argc, char **argv) {
	static const struct option lgopts[] = {
		{ "cli-host", required_argument, 0, 1000 },
		{ "cli-port", required_argument, 0, 1001 },
		{NULL, 0, 0, 0}
	};
	int opt;
	while ((opt = getopt_long(argc, argv, "", lgopts, NULL)) != EOF) {
		switch (opt) {
		case 1000:
			cli_host = optarg;
			break;
		case 1001: {
			char *endp = NULL;
			long v = strtol(optarg, &endp, 10);
			if (!optarg[0] || (endp && *endp) || v <= 0 || v > 65535) {
				return -1;
			}
			cli_port = (uint16_t)v;
			break;
		}
		default:
			return -1;
		}
	}
	return 0;
}

static int start_cli_server(void) {
	int srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0) {
		return -1;
	}
	int opt = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(cli_port);
	if (inet_aton(cli_host, &addr.sin_addr) == 0) {
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}

	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(srv);
		return -1;
	}
	if (listen(srv, 4) < 0) {
		close(srv);
		return -1;
	}

	printf("ACL CLI listening on %s:%u\n", cli_host, cli_port);
	printf("Connect with: nc %s %u\n", cli_host, cli_port);

	while (!force_quit) {
		int cfd = accept(srv, NULL, NULL);
		if (cfd < 0) {
			if (force_quit) {
				break;
			}
			continue;
		}
		struct cmdline *cl = cmdline_new(acl_cmdline_ctx, prompt, cfd, cfd);
		if (!cl) {
			close(cfd);
			continue;
		}
		cmdline_printf(cl, "%s", welcome);
		cmdline_interact(cl);
		cmdline_free(cl);
		close(cfd);
	}
	close(srv);
	return 0;
}

int main(int argc, char **argv) {
	int eal_args = rte_eal_init(argc, argv);
	if (eal_args < 0) {
		return -1;
	}
	argc -= eal_args;
	argv += eal_args;
	if (rte_eal_process_type() != RTE_PROC_SECONDARY) {
		return -1;
	}

	if (parse_cli_args(argc, argv) != 0) {
		return -1;
	}

	acl_cmd_ring = rte_ring_lookup(ACL_CMD_RING_NAME);
	acl_resp_ring = rte_ring_lookup(ACL_RESP_RING_NAME);
	if (!acl_cmd_ring || !acl_resp_ring) {
		return -1;
	}
	const struct rte_memzone *mz = rte_memzone_lookup(ACL_SHARED_CFG_NAME);
	if (!mz) {
		return -1;
	}

	acl_shared_cfg = (struct acl_shared_cfg *)mz->addr;
	const struct rte_memzone *hitz = rte_memzone_lookup(ACL_HIT_SHARED_NAME);
	if (hitz) {
		acl_hit_shared_cfg = (struct acl_hit_shared_cfg *)hitz->addr;
	}
	acl6_cmd_ring = rte_ring_lookup(ACL6_CMD_RING_NAME);
	acl6_resp_ring = rte_ring_lookup(ACL6_RESP_RING_NAME);
	if (!acl6_cmd_ring || !acl6_resp_ring) {
		return -1;
	}
	const struct rte_memzone *mz6 = rte_memzone_lookup(ACL6_SHARED_CFG_NAME);
	if (!mz6) {
		return -1;
	}
	acl6_shared_cfg = (struct acl6_shared_cfg *)mz6->addr;
	const struct rte_memzone *hitz6 = rte_memzone_lookup(ACL6_HIT_SHARED_NAME);
	if (hitz6) {
		acl6_hit_shared_cfg = (struct acl6_hit_shared_cfg *)hitz6->addr;
	}
	const struct rte_memzone *smz = rte_memzone_lookup(SESSION_SHARED_NAME);
	if (smz) {
		session_shared_cfg = (struct session_shared_cfg *)smz->addr;
	}
	const struct rte_memzone *smz6 = rte_memzone_lookup(SESSION6_SHARED_NAME);
	if (smz6) {
		session6_shared_cfg = (struct session6_shared_cfg *)smz6->addr;
	}
	const struct rte_memzone *pmz = rte_memzone_lookup(PORTSTATS_SHARED_NAME);
	if (pmz) {
		portstats_shared_cfg = (struct portstats_shared_cfg *)pmz->addr;
	}
	const struct rte_memzone *dmz = rte_memzone_lookup(DENYLOG_SHARED_NAME);
	if (dmz) {
		denylog_shared_cfg = (struct denylog_shared_cfg *)dmz->addr;
	}
	const struct rte_memzone *dmz6 = rte_memzone_lookup(DENYLOG6_SHARED_NAME);
	if (dmz6) {
		denylog6_shared_cfg = (struct denylog6_shared_cfg *)dmz6->addr;
	}
	const struct rte_memzone *rt6 = rte_memzone_lookup(ROUTE6_SHARED_NAME);
	if (rt6) {
		route6_shared_cfg = (struct route6_shared_cfg *)rt6->addr;
	}
	const struct rte_memzone *amz = rte_memzone_lookup(ATTACK_SHARED_NAME);
	if (amz) {
		attack_shared_cfg = (struct attack_shared_cfg *)amz->addr;
	}
	const struct rte_memzone *rmz = rte_memzone_lookup(RLIM_SHARED_NAME);
	if (rmz) {
		rlim_shared_cfg = (struct rlim_shared_cfg *)rmz->addr;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	return start_cli_server();
}
