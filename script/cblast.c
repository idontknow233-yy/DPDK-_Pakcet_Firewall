#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>

static volatile int quit;

static void sig_handler(int s) { quit = 1; }

static int parse_mac(const char *s, unsigned char *m) {
    unsigned b[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x", &b[0],&b[1],&b[2],&b[3],&b[4],&b[5]) != 6) return -1;
    for (int i = 0; i < 6; i++) m[i] = (unsigned char)b[i];
    return 0;
}

static uint16_t ip_cksum(const uint8_t *hdr, int len) {
    uint32_t s = 0;
    for (int i = 0; i < len; i += 2)
        s += ((uint16_t)hdr[i] << 8) + (i + 1 < len ? hdr[i + 1] : 0);
    while (s >> 16) s = (s & 0xffff) + (s >> 16);
    return (uint16_t)(~s & 0xffff);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <pkt_size> <dst_mac> [duration_sec] [iface]\n", argv[0]);
        return 1;
    }

    uint16_t pkt_size = (uint16_t)atoi(argv[1]);
    int duration = argc > 3 ? atoi(argv[3]) : 30;
    const char *iface = argc > 4 ? argv[4] : "ens224";

    if (pkt_size < 64) pkt_size = 64;

    uint8_t frame[2048];
    uint8_t dst_mac[6], src_mac[6];

    memset(dst_mac, 0, 6);
    parse_mac(argv[2], dst_mac);

    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) { perror("socket"); return 1; }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ioctl(sock, SIOCGIFINDEX, &ifr);
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = ifr.ifr_ifindex;
    bind(sock, (struct sockaddr *)&sll, sizeof(sll));

    ioctl(sock, SIOCGIFHWADDR, &ifr);
    memcpy(src_mac, ifr.ifr_hwaddr.sa_data, 6);

    /* Build packet template */
    uint16_t ip_payload = pkt_size - 14 - 20 - 8;
    uint8_t *p = frame;

    /* Ethernet */
    memcpy(p, dst_mac, 6); p += 6;
    memcpy(p, src_mac, 6); p += 6;
    *p++ = 0x08; *p++ = 0x00; /* IPv4 */

    /* IP header */
    *p++ = 0x45;
    *p++ = 0x00;
    uint16_t total_len = 20 + 8 + ip_payload;
    *p++ = (uint8_t)(total_len >> 8);
    *p++ = (uint8_t)(total_len & 0xff);
    *p++ = 0x00; *p++ = 0x01; /* ID */
    *p++ = 0x00; *p++ = 0x00; /* Flags/frag */
    *p++ = 64; /* TTL */
    *p++ = 17; /* UDP */
    /* checksum placeholder */
    uint8_t *cksum_pos = p;
    *p++ = 0x00; *p++ = 0x00;
    /* src IP */
    *p++ = 10; *p++ = 0; *p++ = 0; *p++ = 1;
    /* dst IP */
    *p++ = 10; *p++ = 0; *p++ = 0; *p++ = 2;

    /* compute IP checksum */
    uint16_t cs = ip_cksum(frame + 14, 20);
    cksum_pos[0] = (uint8_t)(cs >> 8);
    cksum_pos[1] = (uint8_t)(cs & 0xff);

    /* UDP header */
    *p++ = 0x30; *p++ = 0x39; /* src port 12345 */
    *p++ = 0x00; *p++ = 0x50; /* dst port 80 */
    uint16_t udp_len = 8 + ip_payload;
    *p++ = (uint8_t)(udp_len >> 8);
    *p++ = (uint8_t)(udp_len & 0xff);
    *p++ = 0x00; *p++ = 0x00; /* checksum 0 */

    /* payload */
    memset(p, 0, ip_payload);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    fprintf(stderr, "[CBLAST] pkt_size=%u dst=%02x:%02x:%02x:%02x:%02x:%02x iface=%s dur=%d\n",
            pkt_size, dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3], dst_mac[4], dst_mac[5], iface, duration);

    uint64_t total = 0, prev = 0;
    int errors = 0;
    time_t t0 = time(NULL), ts = t0;
    int batch = 64;

    while (!quit) {
        for (int i = 0; i < batch; i++) {
            ssize_t n = send(sock, frame, pkt_size, 0);
            if (n > 0) total++;
            else if (errors == 0) { perror("send"); errors = 1; }
            else errors++;
        }

        if (time(NULL) != ts) {
            ts = time(NULL);
            double dt = (double)(ts - t0);
            double pps = (double)(total - prev) / (double)(ts - t0 + 1 - (t0 == ts ? 0 : (time_t)((uint64_t)(ts - t0))));
            if (dt > 0 && ts > t0) {
                uint64_t delta = total - prev;
                printf("CB: %lu total | %lu pkts/s | %.1f Mbps%s\n",
                       (unsigned long)total, (unsigned long)delta, delta * pkt_size * 8 / 1e6,
                       errors ? " (errors!)" : "");
                fflush(stdout);
            }
            prev = total;
        }

        if (duration > 0 && time(NULL) - t0 >= duration) break;
    }

    printf("CB_DONE: %lu\n", (unsigned long)total);
    fflush(stdout);
    close(sock);
    return 0;
}
