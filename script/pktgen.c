#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <arpa/inet.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_cycles.h>
#include <rte_launch.h>
#include <rte_lcore.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 64
#define TX_QUEUES 1
#define TX_RING_SIZE 1024

static volatile bool force_quit;

static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        force_quit = true;
    }
}

static struct rte_mempool *pktmbuf_pool;

struct pktgen_config {
    struct rte_ether_addr dst_mac;
    struct rte_ether_addr src_mac;
    uint16_t pkt_size;
    uint32_t dst_ip;
    uint32_t src_ip;
    uint16_t dst_port;
    uint16_t src_port;
    uint16_t portid;
};

static void build_packet(struct rte_mbuf *m, struct pktgen_config *cfg) {
    struct rte_ether_hdr *eth;
    struct rte_ipv4_hdr *ip;
    struct rte_udp_hdr *udp;
    uint8_t *payload;

    uint16_t eth_size = sizeof(struct rte_ether_hdr);
    uint16_t ip_size = sizeof(struct rte_ipv4_hdr);
    uint16_t udp_size = sizeof(struct rte_udp_hdr);
    uint16_t payload_size = cfg->pkt_size - eth_size - ip_size - udp_size;

    m->data_len = cfg->pkt_size;
    m->pkt_len = cfg->pkt_size;

    eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    rte_ether_addr_copy(&cfg->dst_mac, &eth->dst_addr);
    rte_ether_addr_copy(&cfg->src_mac, &eth->src_addr);
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    ip = (struct rte_ipv4_hdr *)(eth + 1);
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = rte_cpu_to_be_16((uint16_t)(ip_size + udp_size + payload_size));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_UDP;
    ip->src_addr = cfg->src_ip;
    ip->dst_addr = cfg->dst_ip;
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    udp = (struct rte_udp_hdr *)(ip + 1);
    udp->src_port = rte_cpu_to_be_16(cfg->src_port);
    udp->dst_port = rte_cpu_to_be_16(cfg->dst_port);
    udp->dgram_len = rte_cpu_to_be_16((uint16_t)(udp_size + payload_size));
    udp->dgram_cksum = 0;

    payload = (uint8_t *)(udp + 1);
    for (uint16_t i = 0; i < payload_size; i++) {
        payload[i] = (uint8_t)(i & 0xff);
    }
}

static void fill_packet_bulk(struct rte_mbuf **pkts, unsigned n,
                              struct pktgen_config *cfg) {
    for (unsigned i = 0; i < n; i++) {
        build_packet(pkts[i], cfg);
    }
}

static int pktgen_worker(void *arg) {
    struct pktgen_config *cfg = (struct pktgen_config *)arg;
    unsigned lcore_id = rte_lcore_id();
    uint16_t portid = cfg->portid;

    if (lcore_id != rte_get_main_lcore()) {
        printf("Pktgen on lcore %u, port %u\n", lcore_id, portid);
    }

    uint64_t pkts_tx = 0;
    uint64_t prev_tsc = rte_rdtsc();
    uint64_t stat_tsc = prev_tsc;
    uint64_t prev_pkts = 0;
    uint64_t tsc_hz = rte_get_tsc_hz();

    while (!force_quit) {
        struct rte_mbuf *pkts[BURST_SIZE];
        int nb_alloc = rte_pktmbuf_alloc_bulk(pktmbuf_pool, pkts, BURST_SIZE);
        if (nb_alloc == 0) {
            continue;
        }

        fill_packet_bulk(pkts, (unsigned)nb_alloc, cfg);

        uint16_t sent = rte_eth_tx_burst(portid, 0, pkts, (uint16_t)nb_alloc);

        if (sent < nb_alloc) {
            for (uint16_t i = sent; i < nb_alloc; i++) {
                rte_pktmbuf_free(pkts[i]);
            }
        }

        pkts_tx += sent;

        uint64_t now = rte_rdtsc();
        if (now - stat_tsc >= tsc_hz) {
            double sec = (double)(now - stat_tsc) / tsc_hz;
            double pps = (double)(pkts_tx - prev_pkts) / sec;
            double mbps = pps * cfg->pkt_size * 8 / 1e6;
            printf("[PKTGEN] %" PRIu64 " pkts | %" PRIu64 " total | %.0f pps | %.1f Mbps\n",
                   pkts_tx - prev_pkts, pkts_tx, pps, mbps);
            prev_pkts = pkts_tx;
            stat_tsc = now;
        }
    }

    printf("[PKTGEN] Final: %" PRIu64 " total packets\n", pkts_tx);
    return 0;
}

int main(int argc, char **argv) {
struct pktgen_config cfg;
uint16_t nb_ports;
uint16_t portid;

    memset(&cfg, 0, sizeof(cfg));
    cfg.pkt_size = 1400;
    cfg.src_ip = rte_cpu_to_be_32(0x0a000001);
    cfg.dst_ip = rte_cpu_to_be_32(0x0a000002);
    cfg.src_port = 12345;
    cfg.dst_port = 80;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");
    argc -= ret;
    argv += ret;

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No ports available\n");

    portid = 0;
    if (getenv("PKTGEN_PORT")) portid = (uint16_t)atoi(getenv("PKTGEN_PORT"));
    if (getenv("PKTGEN_SIZE")) cfg.pkt_size = (uint16_t)atoi(getenv("PKTGEN_SIZE"));
    if (getenv("PKTGEN_DSTMAC")) {
        unsigned b[6];
        if (sscanf(getenv("PKTGEN_DSTMAC"), "%02x:%02x:%02x:%02x:%02x:%02x",
                   &b[0],&b[1],&b[2],&b[3],&b[4],&b[5]) == 6) {
            for (int i = 0; i < 6; i++) cfg.dst_mac.addr_bytes[i] = (uint8_t)b[i];
        }
    }
    if (getenv("PKTGEN_DSTIP")) cfg.dst_ip = rte_cpu_to_be_32(strtoul(getenv("PKTGEN_DSTIP"), NULL, 0));

    if (portid >= nb_ports)
        rte_exit(EXIT_FAILURE, "Port %u not available\n", portid);

    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(portid, &dev_info);
    rte_eth_macaddr_get(portid, &cfg.src_mac);

    printf("[PKTGEN] port=%u src_mac=%02x:%02x:%02x:%02x:%02x:%02x dst_mac=%02x:%02x:%02x:%02x:%02x:%02x pkt_size=%u\n",
           portid,
           cfg.src_mac.addr_bytes[0], cfg.src_mac.addr_bytes[1],
           cfg.src_mac.addr_bytes[2], cfg.src_mac.addr_bytes[3],
           cfg.src_mac.addr_bytes[4], cfg.src_mac.addr_bytes[5],
           cfg.dst_mac.addr_bytes[0], cfg.dst_mac.addr_bytes[1],
           cfg.dst_mac.addr_bytes[2], cfg.dst_mac.addr_bytes[3],
           cfg.dst_mac.addr_bytes[4], cfg.dst_mac.addr_bytes[5],
           cfg.pkt_size);

    pktmbuf_pool = rte_pktmbuf_pool_create("pktgen_mbuf_pool", NUM_MBUFS,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!pktmbuf_pool)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    struct rte_eth_conf port_conf = {0};
    port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    port_conf.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;

    uint16_t nb_rxq = 1;
    uint16_t nb_txq = TX_QUEUES;

    ret = rte_eth_dev_configure(portid, nb_rxq, nb_txq, &port_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure port %u: %d\n", portid, ret);

    uint16_t rx_desc = 256;
    uint16_t tx_desc = TX_RING_SIZE;
    rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rx_desc, &tx_desc);

    ret = rte_eth_rx_queue_setup(portid, 0, rx_desc,
        rte_eth_dev_socket_id(portid), NULL, pktmbuf_pool);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot setup RX queue: %d\n", ret);

    struct rte_eth_txconf tx_conf = dev_info.default_txconf;
    ret = rte_eth_tx_queue_setup(portid, 0, tx_desc,
        rte_eth_dev_socket_id(portid), &tx_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot setup TX queue: %d\n", ret);

    ret = rte_eth_dev_start(portid);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot start port %u: %d\n", portid, ret);

    rte_eth_promiscuous_enable(portid);

    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    cfg.portid = portid;
    rte_eal_mp_remote_launch(pktgen_worker, &cfg, CALL_MAIN);
    pktgen_worker(&cfg);
    rte_eal_mp_wait_lcore();

    rte_eth_dev_stop(portid);
    rte_eth_dev_close(portid);
    rte_mempool_free(pktmbuf_pool);
    rte_eal_cleanup();

    return 0;
}
