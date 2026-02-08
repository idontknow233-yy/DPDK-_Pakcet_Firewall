#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "ipc/acl_ipc.h"

static volatile sig_atomic_t force_quit;
static struct rte_ring *acl_cmd_ring;
static struct rte_ring *acl_resp_ring;
static struct acl_shared_cfg *acl_shared_cfg;
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

struct cmd_acl_quit_result {
	cmdline_fixed_string_t quit;
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
	struct in_addr m;
	addr.s_addr = rte_cpu_to_be_32(ip);
	m.s_addr = rte_cpu_to_be_32(mask);
	char ipbuf[INET_ADDRSTRLEN];
	char maskbuf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr, ipbuf, sizeof(ipbuf));
	inet_ntop(AF_INET, &m, maskbuf, sizeof(maskbuf));
	snprintf(out, len, "%s/%s", ipbuf, maskbuf);
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

static void cmd_acl_quit_parsed(void *parsed_result, struct cmdline *cl, void *data) {
	(void)parsed_result;
	(void)data;
	cmdline_quit(cl);
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

cmdline_parse_ctx_t acl_cmdline_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_acl_add,
	(cmdline_parse_inst_t *)&cmd_acl_del,
	(cmdline_parse_inst_t *)&cmd_acl_clear,
	(cmdline_parse_inst_t *)&cmd_acl_list,
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

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	return start_cli_server();
}
