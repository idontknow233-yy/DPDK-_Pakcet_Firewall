#include "acl_cmdline.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <cmdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_rdline.h>
#include <cmdline_socket.h>

#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_byteorder.h>

#include "acl/acl.h"

static struct acl_ctx *acl_ctx_global;

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

static void cmd_acl_add_parsed(void *parsed_result, struct cmdline *cl, __rte_unused void *data) {
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

	if (acl_add_rule(acl_ctx_global, &rule) != 0) {
		cmdline_printf(cl, "ACL add failed\n");
		return;
	}
	cmdline_printf(cl, "ACL rule added\n");
}

static void cmd_acl_del_parsed(void *parsed_result, struct cmdline *cl, __rte_unused void *data) {
	struct cmd_acl_del_result *res = parsed_result;
	if (acl_delete_rule(acl_ctx_global, res->index) != 0) {
		cmdline_printf(cl, "ACL delete failed\n");
		return;
	}
	cmdline_printf(cl, "ACL rule deleted\n");
}

static void cmd_acl_clear_parsed(void *parsed_result, struct cmdline *cl, __rte_unused void *data) {
	(void)parsed_result;
	acl_clear(acl_ctx_global);
	cmdline_printf(cl, "ACL cleared\n");
}

static void cmd_acl_list_parsed(void *parsed_result, struct cmdline *cl, __rte_unused void *data) {
	(void)parsed_result;
	uint32_t count = acl_count(acl_ctx_global);
	cmdline_printf(cl, "ACL rules: %u\n", count);
	for (uint32_t i = 0; i < count; i++) {
		struct acl_rule rule;
		char srcbuf[64];
		char dstbuf[64];
		if (acl_get_rule(acl_ctx_global, i, &rule) != 0) {
			continue;
		}
		print_ip_mask(srcbuf, sizeof(srcbuf), rule.src_ip, rule.src_mask);
		print_ip_mask(dstbuf, sizeof(dstbuf), rule.dst_ip, rule.dst_mask);
		if (rule.match_ports) {
			cmdline_printf(cl, "%u %s src=%s dst=%s proto=%u sport=%u-%u dport=%u-%u\n",
				i, rule.allow ? "allow" : "deny",
				srcbuf, dstbuf, rule.proto,
				rule.src_port_min, rule.src_port_max,
				rule.dst_port_min, rule.dst_port_max);
		} else {
			cmdline_printf(cl, "%u %s src=%s dst=%s proto=any ports=any\n",
				i, rule.allow ? "allow" : "deny", srcbuf, dstbuf);
		}
	}
}

static void cmd_acl_quit_parsed(void *parsed_result, struct cmdline *cl, __rte_unused void *data) {
	(void)parsed_result;
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

static void *acl_cmdline_thread(void *arg) {
	struct acl_ctx *ctx = arg;
	acl_ctx_global = ctx;
	struct cmdline *cl = cmdline_stdin_new(acl_cmdline_ctx, "dpdk-acl> ");
	if (!cl) {
		return NULL;
	}
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
	return NULL;
}

int acl_cmdline_start(struct acl_ctx *ctx, pthread_t *thread) {
	if (!ctx || !thread) {
		return -EINVAL;
	}
	int ret = pthread_create(thread, NULL, acl_cmdline_thread, ctx);
	if (ret != 0) {
		return -ret;
	}
	return 0;
}

void acl_cmdline_stop(pthread_t *thread) {
	if (!thread) {
		return;
	}
	if (*thread) {
		pthread_cancel(*thread);
		pthread_join(*thread, NULL);
		*thread = 0;
	}
}
