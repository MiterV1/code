#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_tun.h>

#define IF_NAMESIZE 16


struct eth_hdr {
    uint8_t  dmac[6];
    uint8_t  smac[6];
    uint16_t ethertype;
    uint8_t  payload[];
} __attribute__((packed));

static inline struct eth_hdr *eth_hdr(unsigned char *skb)
{
    struct eth_hdr *hdr = (struct eth_hdr *)skb;

    hdr->ethertype = ntohs(hdr->ethertype);

    return hdr;
}

typedef struct {
    char name[IF_NAMESIZE];
    int  tfd;                  /**< file descriptor for tap interface*/
    int  sfd;                  /**< socket descriptor */
    int  mtu;                  /**< cached mtu */
    unsigned char mac[ETH_ALEN]; /**< MAC address of pktio side */
} pktio_tap_t;

static unsigned char mac_addr[ETH_ALEN] = {
    0x2e, 0x15, 0xb9, 0x5b, 0x44, 0x7c
};

static int tap_open(pktio_tap_t *tap)
{
    int ret;
    static int i = 0;
    struct ifreq ifr = {0};

    tap->tfd = open("/dev/net/tun", O_RDWR);
    if (tap->tfd < 0) {
        perror("failed to open /dev/net/tun\n");
        return -1;
    }

    sprintf(tap->name, "tap%d", i++);
    memcpy(tap->mac, mac_addr, ETH_ALEN);

    /*
     * Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *        IFF_NO_PI - Do not provide packet information
     */
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    ret = ioctl(tap->tfd, TUNSETIFF, (void *)&ifr);
    if (ret < 0) {
        printf("%s: creating tap device failed\n", ifr.ifr_name);
        goto tap_err;
    }

    /* Create AF_INET socket for network interface related operations. */
    tap->sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tap->sfd < 0) {
        perror("socket creation failed\n");
        goto tap_err;
    }

    return 0;

tap_err:
    close(tap->tfd);
    perror("Tap device alloc failed.\n");

    return -1;
}

static int tap_setmac(pktio_tap_t *tap)
{
    int ret;
    struct ifreq ethreq = {0};

    snprintf(ethreq.ifr_name, IF_NAMESIZE, "%s", tap->name);
    ethreq.ifr_hwaddr.sa_family = AF_UNIX;
    memcpy(ethreq.ifr_hwaddr.sa_data, tap->mac, ETH_ALEN);
    ret = ioctl(tap->sfd, SIOCSIFHWADDR, &ethreq);
    if (ret != 0) {
        printf("ioctl(SIOCSIFHWADDR): %s: \"%s\".\n", strerror(errno),
            ethreq.ifr_name);
        return -1;
    }

    return 0;
}

static int tap_start(pktio_tap_t *tap)
{
    struct ifreq ifr = {0};

    snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", (char *)tap->name);

    /* Up interface by default. */
    if (ioctl(tap->sfd, SIOCGIFFLAGS, &ifr) < 0) {
        printf("ioctl(SIOCGIFFLAGS) failed: %s\n", strerror(errno));
        goto sock_err;
    }

    ifr.ifr_flags |= IFF_UP;
    ifr.ifr_flags |= IFF_RUNNING;

    if (ioctl(tap->sfd, SIOCSIFFLAGS, &ifr) < 0) {
        printf("failed to come up: %s\n", strerror(errno));
        goto sock_err;
    }

    return 0;

sock_err:
    printf("Tap device start failed.\n");

    return -1;
}


static int netdev_receive(uint8_t *skb)
{
    struct eth_hdr *hdr = eth_hdr(skb);

    switch (hdr->ethertype) {
        case ETH_P_ARP:
            arp_rcv(skb);
            break;
        default:
            printf("Unsupported ethertype %x\n", hdr->ethertype);
            break;
    }

    return 0;
}

static int tap_recv(pktio_tap_t *tap, int num)
{
    int i;
    size_t retval;
    uint8_t buf[4096];

    for (i = 0; i < num; i++) {
        do {
            retval = read(tap->tfd, buf, 4096);
        } while (retval < 0 && errno == EINTR);

        if (retval < 0) {
            break;
        }
    }

    return i;
}

int main(void)
{
    pktio_tap_t tap;

    tap_open(&tap);
    tap_setmac(&tap);
    tap_start(&tap);

    while (1) {
        tap_recv(&tap, 32);
    }
}

#if 0
static int tap_pktio_stop(pktio_entry_t *pktio_entry)
{
    struct ifreq ifr;
    pkt_tap_t *tap = pkt_priv(pktio_entry);

    odp_memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IF_NAMESIZE, "%s",
         (char *)pktio_entry->s.name + 4);

        /* Up interface by default. */
    if (ioctl(tap->skfd, SIOCGIFFLAGS, &ifr) < 0) {
        __odp_errno = errno;
        ODP_ERR("ioctl(SIOCGIFFLAGS) failed: %s\n", strerror(errno));
        goto sock_err;
    }

    ifr.ifr_flags &= ~IFF_UP;
    ifr.ifr_flags &= ~IFF_RUNNING;

    if (ioctl(tap->skfd, SIOCSIFFLAGS, &ifr) < 0) {
        __odp_errno = errno;
        ODP_ERR("failed to come up: %s\n", strerror(errno));
        goto sock_err;
    }

    return 0;
sock_err:
    ODP_ERR("Tap device open failed.\n");
    return -1;
}

static int tap_pktio_close(pktio_entry_t *pktio_entry)
{
    int ret = 0;
    pkt_tap_t *tap = pkt_priv(pktio_entry);

    if (tap->fd != -1 && close(tap->fd) != 0) {
        __odp_errno = errno;
        ODP_ERR("close(tap->fd): %s\n", strerror(errno));
        ret = -1;
    }

    if (tap->skfd != -1 && close(tap->skfd) != 0) {
        __odp_errno = errno;
        ODP_ERR("close(tap->skfd): %s\n", strerror(errno));
        ret = -1;
    }

    return ret;
}

static odp_packet_t pack_odp_pkt(pktio_entry_t *pktio_entry, const void *data,
                 unsigned int len, odp_time_t *ts)
{
    odp_packet_t pkt;
    odp_packet_hdr_t *pkt_hdr;
    odp_packet_hdr_t parsed_hdr;
    int num;

    if (pktio_cls_enabled(pktio_entry)) {
        if (cls_classify_packet(pktio_entry, data, len, len,
                    &pkt_priv(pktio_entry)->pool,
                    &parsed_hdr, true)) {
            return ODP_PACKET_INVALID;
        }
    }

    num = packet_alloc_multi(pkt_priv(pktio_entry)->pool, len, &pkt, 1);

    if (num != 1)
        return ODP_PACKET_INVALID;

    if (odp_packet_copy_from_mem(pkt, 0, len, data) < 0) {
        ODP_ERR("failed to copy packet data\n");
        odp_packet_free(pkt);
        return ODP_PACKET_INVALID;
    }

    pkt_hdr = packet_hdr(pkt);

    if (pktio_cls_enabled(pktio_entry))
        copy_packet_cls_metadata(&parsed_hdr, pkt_hdr);
    else
        packet_parse_layer(pkt_hdr,
                   pktio_entry->s.config.parser.layer,
                   pktio_entry->s.in_chksums);

    packet_set_ts(pkt_hdr, ts);
    pkt_hdr->input = pktio_entry->s.handle;

    return pkt;
}

static int tap_pktio_send_lockless(pktio_entry_t *pktio_entry,
                   const odp_packet_t pkts[], int num)
{
    ssize_t retval;
    int i, n;
    uint32_t pkt_len;
    uint8_t buf[BUF_SIZE];
    pkt_tap_t *tap = pkt_priv(pktio_entry);

    for (i = 0; i < num; i++) {
        pkt_len = odp_packet_len(pkts[i]);

        if (pkt_len > tap->mtu) {
            if (i == 0) {
                __odp_errno = EMSGSIZE;
                return -1;
            }
            break;
        }

        if (odp_packet_copy_to_mem(pkts[i], 0, pkt_len, buf) < 0) {
            ODP_ERR("failed to copy packet data\n");
            break;
        }

        do {
            retval = write(tap->fd, buf, pkt_len);
        } while (retval < 0 && errno == EINTR);

        if (retval < 0) {
            if (i == 0 && SOCK_ERR_REPORT(errno)) {
                __odp_errno = errno;
                ODP_ERR("write(): %s\n", strerror(errno));
                return -1;
            }
            break;
        } else if ((uint32_t)retval != pkt_len) {
            ODP_ERR("sent partial ethernet packet\n");
            if (i == 0) {
                __odp_errno = EMSGSIZE;
                return -1;
            }
            break;
        }
    }

    for (n = 0; n < i; n++)
        odp_packet_free(pkts[n]);

    return i;
}

static int tap_pktio_send(pktio_entry_t *pktio_entry, int index ODP_UNUSED,
              const odp_packet_t pkts[], int num)
{
    int ret;

    odp_ticketlock_lock(&pktio_entry->s.txl);

    ret = tap_pktio_send_lockless(pktio_entry, pkts, num);

    odp_ticketlock_unlock(&pktio_entry->s.txl);

    return ret;
}

static uint32_t tap_mtu_get(pktio_entry_t *pktio_entry)
{
    uint32_t ret;

    ret =  mtu_get_fd(pkt_priv(pktio_entry)->skfd,
              pktio_entry->s.name + 4);
    if (ret > 0)
        pkt_priv(pktio_entry)->mtu = ret;

    return ret;
}

static int tap_promisc_mode_set(pktio_entry_t *pktio_entry,
                odp_bool_t enable)
{
    return promisc_mode_set_fd(pkt_priv(pktio_entry)->skfd,
                   pktio_entry->s.name + 4, enable);
}

static int tap_promisc_mode_get(pktio_entry_t *pktio_entry)
{
    return promisc_mode_get_fd(pkt_priv(pktio_entry)->skfd,
                   pktio_entry->s.name + 4);
}

static int tap_mac_addr_get(pktio_entry_t *pktio_entry, void *mac_addr)
{
    memcpy(mac_addr, pkt_priv(pktio_entry)->if_mac, ETH_ALEN);
    return ETH_ALEN;
}

static int tap_mac_addr_set(pktio_entry_t *pktio_entry, const void *mac_addr)
{
    pkt_tap_t *tap = pkt_priv(pktio_entry);

    memcpy(tap->if_mac, mac_addr, ETH_ALEN);

    return mac_addr_set_fd(tap->fd, (char *)pktio_entry->s.name + 4,
              tap->if_mac);
}

static int tap_link_status(pktio_entry_t *pktio_entry)
{
    return link_status_fd(pkt_priv(pktio_entry)->skfd,
                  pktio_entry->s.name + 4);
}

static int tap_capability(pktio_entry_t *pktio_entry ODP_UNUSED,
              odp_pktio_capability_t *capa)
{
    memset(capa, 0, sizeof(odp_pktio_capability_t));

    capa->max_input_queues  = 1;
    capa->max_output_queues = 1;
    capa->set_op.op.promisc_mode = 1;
    capa->set_op.op.mac_addr = 1;

    odp_pktio_config_init(&capa->config);
    capa->config.pktin.bit.ts_all = 1;
    capa->config.pktin.bit.ts_ptp = 1;
    return 0;
}

const pktio_if_ops_t tap_pktio_ops = {
    .name = "tap",
    .print = NULL,
    .init_global = NULL,
    .init_local = NULL,
    .term = NULL,
    .open = tap_pktio_open,
    .close = tap_pktio_close,
    .start = tap_pktio_start,
    .stop = tap_pktio_stop,
    .recv = tap_pktio_recv,
    .send = tap_pktio_send,
    .mtu_get = tap_mtu_get,
    .promisc_mode_set = tap_promisc_mode_set,
    .promisc_mode_get = tap_promisc_mode_get,
    .mac_get = tap_mac_addr_get,
    .mac_set = tap_mac_addr_set,
    .link_status = tap_link_status,
    .capability = tap_capability,
    .pktin_ts_res = NULL,
    .pktin_ts_from_ns = NULL,
    .config = NULL
};
#endif
