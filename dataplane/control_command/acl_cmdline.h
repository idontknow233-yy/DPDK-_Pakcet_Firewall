#ifndef DPDK_PF_ACL_CMDLINE_H
#define DPDK_PF_ACL_CMDLINE_H

#include <pthread.h>

struct acl_ctx;

int acl_cmdline_start(struct acl_ctx *ctx, pthread_t *thread);
void acl_cmdline_stop(pthread_t *thread);

#endif
