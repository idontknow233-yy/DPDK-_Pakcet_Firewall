#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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

#define NUM_MBUFS 16384
#define MBUF_CACHE 256
#define TX_BURST 64

static volatile int quit;

static void sig_handler(int s) { quit = 1; }

struct pkt_cfg {
	uint16_t port;
	uint16_t pkt_size;
	int duration_sec;
	struct rte_ether_addr dst_mac;
	struct rte_ether_addr src_mac;
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
};

static struct rte_mempool *pool;

static void
build_pkt(struct rte_mbuf *m, struct pkt_cfg *c)
{
	m->data_len = m->pkt_len = c->pkt_size;

	struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m,
		struct rte_ether_hdr *);
	rte_ether_addr_copy(&c->dst_mac, &eth->dst_addr);
	rte_ether_addr_copy(&c->src_mac, &eth->src_addr);
	eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

	struct rte_ipv4_hdr *ip4 = (struct rte_ipv4_hdr *)(eth + 1);
	uint16_t pl = c->pkt_size - (uint16_t)(sizeof(*eth) +
		sizeof(*ip4) + sizeof(struct rte_udp_hdr));

	memset(ip4, 0, sizeof(*ip4));
	ip4->version_ihl = 0x45;
	ip4->total_length = rte_cpu_to_be_16(
		(uint16_t)(sizeof(*ip4) + sizeof(struct rte_udp_hdr) + pl));
	ip4->time_to_live = 64;
	ip4->next_proto_id = IPPROTO_UDP;
	ip4->src_addr = c->src_ip;
	ip4->dst_addr = c->dst_ip;
	ip4->hdr_checksum = rte_ipv4_cksum(ip4);

	struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip4 + 1);
	udp->src_port = rte_cpu_to_be_16(c->src_port);
	udp->dst_port = rte_cpu_to_be_16(c->dst_port);
	udp->dgram_len = rte_cpu_to_be_16(
		(uint16_t)(sizeof(*udp) + pl));
	udp->dgram_cksum = 0;
}

static int
parse_mac(const char *s, struct rte_ether_addr *m)
{
	unsigned b[6];
	if (sscanf(s, "%x:%x:%x:%x:%x:%x",
	           &b[0],&b[1],&b[2],&b[3],&b[4],&b[5]) != 6)
		return -1;
	for (int i = 0; i < 6; i++)
		m->addr_bytes[i] = (uint8_t)b[i];
	return 0;
}

int main(int argc, char **argv)
{
	struct pkt_cfg c;
	memset(&c, 0, sizeof(c));
	c.pkt_size = 64;
	c.duration_sec = 30;
	c.src_ip = rte_cpu_to_be_32(0x0a000001);
	c.dst_ip = rte_cpu_to_be_32(0x0a000002);
	c.src_port = 12345;
	c.dst_port = 80;

	int ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		fprintf(stderr, "EAL init failed\n");
		return 1;
	}
	argc -= ret; argv += ret;

	if (argc > 0) c.pkt_size = (uint16_t)atoi(argv[0]);
	if (argc > 1) c.duration_sec = atoi(argv[1]);
	if (argc > 2) parse_mac(argv[2], &c.dst_mac);

	uint16_t nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0) {
		fprintf(stderr, "No ports found (check --vdev)\n");
		return 1;
	}
	c.port = 0;

	struct rte_eth_dev_info info;
	rte_eth_dev_info_get(c.port, &info);

	struct rte_eth_conf cfg = {0};
	cfg.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;
	cfg.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;

	if (rte_eth_dev_configure(c.port, 1, 1, &cfg) < 0) {
		fprintf(stderr, "dev_configure failed\n");
		return 1;
	}

	uint16_t rx_desc = 128, tx_desc = 1024;
	rte_eth_dev_adjust_nb_rx_tx_desc(c.port, &rx_desc, &tx_desc);

	pool = rte_pktmbuf_pool_create("pktgen_pool", NUM_MBUFS,
		MBUF_CACHE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (!pool) {
		fprintf(stderr, "pool_create failed\n");
		return 1;
	}

	if (rte_eth_rx_queue_setup(c.port, 0, rx_desc,
		rte_socket_id(), NULL, pool) < 0) {
		fprintf(stderr, "rx_queue_setup failed\n");
		return 1;
	}

	struct rte_eth_txconf txconf = info.default_txconf;
	if (rte_eth_tx_queue_setup(c.port, 0, tx_desc,
		rte_socket_id(), &txconf) < 0) {
		fprintf(stderr, "tx_queue_setup failed\n");
		return 1;
	}

	if (rte_eth_dev_start(c.port) < 0) {
		fprintf(stderr, "dev_start failed\n");
		return 1;
	}

	rte_eth_macaddr_get(c.port, &c.src_mac);

	fprintf(stderr, "PKTGEN: port=%u pkt=%u dst=%02x:%02x:%02x:%02x:%02x:%02x dur=%d\n",
	        c.port, c.pkt_size,
	        c.dst_mac.addr_bytes[0], c.dst_mac.addr_bytes[1],
	        c.dst_mac.addr_bytes[2], c.dst_mac.addr_bytes[3],
	        c.dst_mac.addr_bytes[4], c.dst_mac.addr_bytes[5],
	        c.duration_sec);

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	struct rte_mbuf *pkts[TX_BURST];
	uint64_t total = 0, prev = 0;
	uint64_t t0 = rte_rdtsc(), ts = t0;
	uint64_t hz = rte_get_tsc_hz();

	while (!quit) {
		int i;
		for (i = 0; i < TX_BURST; i++) {
			pkts[i] = rte_pktmbuf_alloc(pool);
			if (!pkts[i]) break;
			build_pkt(pkts[i], &c);
		}
		int nb = i;
		if (nb == 0) continue;

		uint16_t sent = rte_eth_tx_burst(c.port, 0, pkts,
		                                 (uint16_t)nb);
		for (uint16_t j = sent; j < (uint16_t)nb; j++)
			rte_pktmbuf_free(pkts[j]);
		total += sent;

		if (rte_rdtsc() - ts >= hz) {
			uint64_t delta = total - prev;
			double dt = (double)(rte_rdtsc() - ts) / hz;
			double pps = delta / dt;
			double mbps = pps * c.pkt_size * 8 / 1e6;
			printf("PKTGEN: %lu pps | %.1f Mbps | %lu total\n",
			       (unsigned long)pps, mbps, (unsigned long)total);
			fflush(stdout);
			prev = total;
			ts = rte_rdtsc();
		}

		if (c.duration_sec > 0 &&
		    (rte_rdtsc() - t0) / hz >= (uint64_t)c.duration_sec)
			break;
	}

	printf("PKTGEN_DONE: %lu\n", (unsigned long)total);
	fflush(stdout);

	rte_eth_dev_stop(c.port);
	rte_eth_dev_close(c.port);
	rte_eal_cleanup();
	return 0;
}
