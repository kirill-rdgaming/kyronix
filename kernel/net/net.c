#include "net.h"
#include "../arch/x86_64/pit.h"
#include "../drivers/netdev.h"
#include "../drivers/virtio_net.h"
#include "../lib/log.h"
#include "../mm/heap.h"

#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

err_t kyronix_netif_init(struct netif *nif);
void kyronix_netif_input(struct netif *nif, const uint8_t *data, uint16_t len);
void kyronix_netif_bind(netdev_t *nd);

static struct netif g_netif;
static bool g_lwip_up;
static bool g_dhcp_active;

struct virtnet_shim {
    netdev_t nd;
};
static struct virtnet_shim g_vnet_shim;

static int vnet_shim_send(netdev_t *nd, const uint8_t *frame, uint16_t len) {
    (void) nd;
    return virtnet_send(frame, len);
}

static void vnet_shim_poll(netdev_t *nd) {
    (void) nd;
    virtnet_poll();
}

static void netif_bring_up(netdev_t *nd, bool use_dhcp) {
    kyronix_netif_bind(nd);
    netif_add(&g_netif, NULL, NULL, NULL, NULL, kyronix_netif_init, ethernet_input);
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    if (use_dhcp) {
        dhcp_start(&g_netif);
        g_dhcp_active = true;
        log_info("net: DHCP started on %s", nd->name);
    } else {
        ip4_addr_t ip, mask, gw;
        IP4_ADDR(&ip, 10, 0, 2, 15);
        IP4_ADDR(&mask, 255, 255, 255, 0);
        IP4_ADDR(&gw, 10, 0, 2, 2);
        netif_set_addr(&g_netif, &ip, &mask, &gw);
        ip4_addr_t dns1;
        IP4_ADDR(&dns1, 10, 0, 2, 3);
        dns_setserver(0, &dns1);
        log_info("net: static IP 10.0.2.15/24 gw 10.0.2.2 on %s", nd->name);
    }
    g_lwip_up = true;
}

void net_init(void) {
    dns_init();

    netdev_t *primary = NULL;
    if (virtnet_ready()) {
        memcpy(g_vnet_shim.nd.name, "vnet0", 6);
        memcpy(g_vnet_shim.nd.mac, virtnet_mac(), 6);
        g_vnet_shim.nd.send = vnet_shim_send;
        g_vnet_shim.nd.poll = vnet_shim_poll;
        g_vnet_shim.nd.priv = NULL;
        netdev_register(&g_vnet_shim.nd);
        primary = &g_vnet_shim.nd;
    }
    if (!primary) primary = netdev_first();
    if (!primary) {
        log_warn("net: no network device available");
        return;
    }

    lwip_init();

    bool use_dhcp = (primary != &g_vnet_shim.nd);
    netif_bring_up(primary, use_dhcp);
}

bool net_dhcp_bound(void) {
    if (!g_dhcp_active) return true;
    return dhcp_supplied_address(&g_netif) != 0;
}

void net_maybe_rebind(void) {
    if (g_lwip_up) return;
    netdev_t *nd = netdev_first();
    if (!nd) return;
    if (!g_lwip_up) {
        lwip_init();
        netif_bring_up(nd, true);
    }
}

void net_receive(const uint8_t *eth_frame, uint16_t len) {
    if (!g_lwip_up) return;
    kyronix_netif_input(&g_netif, eth_frame, len);
}

void net_poll(void) {
    netdev_poll_all();
    static uint8_t s_ctr;
    if (++s_ctr == 0) sys_check_timeouts();
    if (g_dhcp_active && dhcp_supplied_address(&g_netif)) {
        g_dhcp_active = false;
        uint32_t ip = ip4_addr_get_u32(netif_ip4_addr(&g_netif));
        uint32_t gw = ip4_addr_get_u32(netif_ip4_gw(&g_netif));
        log_info("net: DHCP bound, IP %u.%u.%u.%u gw %u.%u.%u.%u", ip & 0xFF, (ip >> 8) & 0xFF,
                 (ip >> 16) & 0xFF, (ip >> 24) & 0xFF, gw & 0xFF, (gw >> 8) & 0xFF,
                 (gw >> 16) & 0xFF, (gw >> 24) & 0xFF);
        uint32_t dns = ip & 0x00FFFFFFu;
        dns |= 0x01000000u;
        ip4_addr_t dns1;
        dns1.addr = gw;
        dns_setserver(0, &dns1);
    }
}
