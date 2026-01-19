#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static volatile bool force_quit;

// 记录每个端口的收包、发包、丢包计数
struct port_stats {
    uint64_t rx; // 接收包计数
    uint64_t tx; // 发送包计数
    uint64_t dropped; // 丢包计数
};

// 每个核的配置
struct lcore_conf {
    uint16_t ports[RTE_MAX_ETHPORTS]; // 该核绑定的端口列表
    uint16_t nb_ports; // 该核绑定的端口数量
};

struct acl_rule {
    uint32_t src_ip;
    uint32_t src_mask;
    uint32_t dst_ip;
    uint32_t dst_mask;
    bool allow;
};

static struct port_stats port_stats[RTE_MAX_ETHPORTS];
static struct lcore_conf lcore_confs[RTE_MAX_LCORE];
static uint16_t port_pair[RTE_MAX_ETHPORTS];

static const struct acl_rule acl_rules[] = {
    {RTE_IPV4(10, 0, 0, 0), RTE_IPV4(255, 0, 0, 0), 0, 0, false},
    {0, 0, 0, 0, true},
};

// 信号处理函数，用于处理SIGINT和SIGTERM信号
// 当接收到这两个信号时，将force_quit标志设置为true，触发主循环退出
static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        force_quit = true;
    }
}

// 检查IP包是否被ACL规则允许
// 遍历所有ACL规则，检查源IP和目的IP是否匹配规则
// 如果匹配且规则允许，则返回true；否则返回false
static bool acl_allow(const struct rte_ipv4_hdr *ip) {
    uint32_t src = rte_be_to_cpu_32(ip->src_addr);
    uint32_t dst = rte_be_to_cpu_32(ip->dst_addr);
    for (uint32_t i = 0; i < sizeof(acl_rules) / sizeof(acl_rules[0]); i++) {
        const struct acl_rule *rule = &acl_rules[i];
        if (rule->src_mask && ((src & rule->src_mask) != (rule->src_ip & rule->src_mask))) {
            continue;
        }
        if (rule->dst_mask && ((dst & rule->dst_mask) != (rule->dst_ip & rule->dst_mask))) {
            continue;
        }
        return rule->allow;
    }
    return true;
}

// 初始化端口
// 配置端口的接收和发送队列，设置端口为混杂模式
// 返回值：0表示成功，其他值表示失败
static int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf = {
        .rxmode = {.mq_mode = RTE_ETH_MQ_RX_NONE},
        .txmode = {.mq_mode = RTE_ETH_MQ_TX_NONE},
    };
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf tx_conf;
    struct rte_eth_rxconf rx_conf;
    int ret;

    ret = rte_eth_dev_info_get(port, &dev_info);
    if (ret != 0) {
        return ret;
    }

    port_conf.rxmode.offloads = dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_CHECKSUM;
    port_conf.txmode.offloads = dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;

    ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
    if (ret < 0) {
        return ret;
    }

    rx_conf = dev_info.default_rxconf;
    rx_conf.offloads = port_conf.rxmode.offloads;
    ret = rte_eth_rx_queue_setup(port, 0, RX_RING_SIZE, rte_eth_dev_socket_id(port), &rx_conf, mbuf_pool);
    if (ret < 0) {
        return ret;
    }

    tx_conf = dev_info.default_txconf;
    tx_conf.offloads = port_conf.txmode.offloads;
    ret = rte_eth_tx_queue_setup(port, 0, TX_RING_SIZE, rte_eth_dev_socket_id(port), &tx_conf);
    if (ret < 0) {
        return ret;
    }

    ret = rte_eth_dev_start(port);
    if (ret < 0) {
        return ret;
    }

    ret = rte_eth_promiscuous_enable(port);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

// 每个核的主循环函数
// 从绑定的端口接收数据包，检查ACL规则，根据规则转发或丢弃数据包
// 返回值：0表示成功，其他值表示失败
static int lcore_main(void *arg) {
    uint16_t port_id;
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t lcore_id = rte_lcore_id();
    struct lcore_conf *conf = &lcore_confs[lcore_id];
    (void)arg;

    while (!force_quit) {
        for (uint16_t i = 0; i < conf->nb_ports; i++) {
            port_id = conf->ports[i];
            uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
            if (nb_rx == 0) {
                continue;
            }
            port_stats[port_id].rx += nb_rx;

            uint16_t tx_port = port_pair[port_id];
            if (tx_port == RTE_MAX_ETHPORTS) {
                for (uint16_t j = 0; j < nb_rx; j++) {
                    rte_pktmbuf_free(bufs[j]);
                }
                port_stats[port_id].dropped += nb_rx;
                continue;
            }

            struct rte_mbuf *tx_bufs[BURST_SIZE];
            uint16_t nb_tx_bufs = 0;

            for (uint16_t j = 0; j < nb_rx; j++) {
                struct rte_mbuf *m = bufs[j];
                if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr)) {
                    rte_pktmbuf_free(m);
                    port_stats[port_id].dropped++;
                    continue;
                }
                struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
                if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                    rte_pktmbuf_free(m);
                    port_stats[port_id].dropped++;
                    continue;
                }
                struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);
                if (ip->time_to_live <= 1) {
                    rte_pktmbuf_free(m);
                    port_stats[port_id].dropped++;
                    continue;
                }
                if (!acl_allow(ip)) {
                    rte_pktmbuf_free(m);
                    port_stats[port_id].dropped++;
                    continue;
                }
                ip->time_to_live--;
                ip->hdr_checksum = 0;
                ip->hdr_checksum = rte_ipv4_cksum(ip);
                tx_bufs[nb_tx_bufs++] = m;
            }

            if (nb_tx_bufs > 0) {
                uint16_t sent = rte_eth_tx_burst(tx_port, 0, tx_bufs, nb_tx_bufs);
                if (sent < nb_tx_bufs) {
                    for (uint16_t k = sent; k < nb_tx_bufs; k++) {
                        rte_pktmbuf_free(tx_bufs[k]);
                    }
                }
                port_stats[port_id].tx += sent;
                port_stats[port_id].dropped += (nb_tx_bufs - sent);
            }
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "EAL init failed\n");
    }

    argc -= ret;
    argv += ret;
    (void)argc;
    (void)argv;

    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    uint16_t nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 1) {
        rte_exit(EXIT_FAILURE, "No available ports\n");
    }

    for (uint16_t port_id = 0; port_id < nb_ports; port_id++) {
        port_pair[port_id] = RTE_MAX_ETHPORTS;
    }
    for (uint16_t port_id = 0; port_id + 1 < nb_ports; port_id += 2) {
        port_pair[port_id] = port_id + 1;
        port_pair[port_id + 1] = port_id;
    }

    uint32_t nb_mbufs = (uint32_t)(nb_ports * (RX_RING_SIZE + TX_RING_SIZE + 1024));
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        nb_mbufs,
        MBUF_CACHE_SIZE,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    }

    for (uint16_t port_id = 0; port_id < nb_ports; port_id++) {
        if (port_init(port_id, mbuf_pool) != 0) {
            rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", port_id);
        }
    }

    unsigned int lcore_id;
    unsigned int lcore_ids[RTE_MAX_LCORE];
    unsigned int lcore_count = 0;
    RTE_LCORE_FOREACH(lcore_id) {
        lcore_confs[lcore_id].nb_ports = 0;
        lcore_ids[lcore_count++] = lcore_id;
    }
    for (uint16_t port_id = 0; port_id < nb_ports; port_id++) {
        unsigned int owner = lcore_ids[port_id % lcore_count];
        lcore_confs[owner].ports[lcore_confs[owner].nb_ports++] = port_id;
    }

    rte_eal_mp_remote_launch(lcore_main, NULL, SKIP_MAIN);
    lcore_main(NULL);
    rte_eal_mp_wait_lcore();

    for (uint16_t port_id = 0; port_id < nb_ports; port_id++) {
        printf("Port %u RX=%" PRIu64 " TX=%" PRIu64 " DROP=%" PRIu64 "\n",
               port_id, port_stats[port_id].rx, port_stats[port_id].tx, port_stats[port_id].dropped);
    }

    for (uint16_t port_id = 0; port_id < nb_ports; port_id++) {
        rte_eth_dev_stop(port_id);
        rte_eth_dev_close(port_id);
    }

    return 0;
}
