#include "netdev.h"
#include "../lib/log.h"
#include "../lib/string.h"

static netdev_t *g_netdevs;
static int g_netdev_count;

void netdev_init(void) {
    g_netdevs = NULL;
    g_netdev_count = 0;
}

int netdev_register(netdev_t *nd) {
    if (g_netdev_count >= NETDEV_MAX) return -1;
    nd->next = g_netdevs;
    g_netdevs = nd;
    g_netdev_count++;
    log_info("netdev: registered %s MAC %02x:%02x:%02x:%02x:%02x:%02x", nd->name, nd->mac[0],
             nd->mac[1], nd->mac[2], nd->mac[3], nd->mac[4], nd->mac[5]);
    return 0;
}

int netdev_count(void) { return g_netdev_count; }

netdev_t *netdev_get(int idx) {
    if (idx < 0 || idx >= g_netdev_count) return NULL;
    netdev_t *nd = g_netdevs;
    for (int i = 0; i < idx && nd; i++) nd = nd->next;
    return nd;
}

netdev_t *netdev_first(void) { return g_netdevs; }

netdev_t *netdev_by_name(const char *name) {
    for (netdev_t *nd = g_netdevs; nd; nd = nd->next)
        if (strcmp(nd->name, name) == 0) return nd;
    return NULL;
}

void net_receive(const uint8_t *eth_frame, uint16_t len);

void netdev_receive(netdev_t *nd, const uint8_t *frame, uint16_t len) {
    (void) nd;
    net_receive(frame, len);
}

void netdev_poll_all(void) {
    for (netdev_t *nd = g_netdevs; nd; nd = nd->next)
        if (nd->poll) nd->poll(nd);
}
