#include <stdbool.h>
#include <stdint.h>

#include "lwip/def.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "netif/ethernet.h"

#include "../../drivers/netdev.h"
#include "../../lib/log.h"
#include "../../lib/string.h"
#include "../../mm/heap.h"

static netdev_t *g_bound_nd;

void kyronix_netif_bind(netdev_t *nd) { g_bound_nd = nd; }
netdev_t *kyronix_netif_dev(void) { return g_bound_nd; }

void kyronix_netif_input(struct netif *nif, const uint8_t *data, uint16_t len) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!p) return;

    struct pbuf *q = p;
    const uint8_t *src = data;
    uint16_t rem = len;
    while (q && rem) {
        uint16_t chunk = (rem < q->len) ? rem : (uint16_t) q->len;
        memcpy(q->payload, src, chunk);
        src += chunk;
        rem -= chunk;
        q = q->next;
    }

    if (nif->input(p, nif) != ERR_OK) pbuf_free(p);
}

static err_t kyronix_netif_output(struct netif *nif, struct pbuf *p) {
    (void) nif;
    if (!g_bound_nd || !g_bound_nd->send) return ERR_IF;

    uint8_t buf[1514];
    uint16_t total = 0;
    for (struct pbuf *q = p; q; q = q->next) {
        if (total + q->len > sizeof(buf)) return ERR_MEM;
        memcpy(buf + total, q->payload, q->len);
        total += (uint16_t) q->len;
    }

    g_bound_nd->send(g_bound_nd, buf, total);
    return ERR_OK;
}

err_t kyronix_netif_init(struct netif *nif) {
    nif->name[0] = 'e';
    nif->name[1] = '0';
    nif->output = etharp_output;
    nif->linkoutput = kyronix_netif_output;
    nif->mtu = 1500;
    nif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;

    if (g_bound_nd) {
        memcpy(nif->hwaddr, g_bound_nd->mac, 6);
        nif->hwaddr_len = 6;
        log_info("net: bound to %s, MAC %02x:%02x:%02x:%02x:%02x:%02x", g_bound_nd->name,
                 g_bound_nd->mac[0], g_bound_nd->mac[1], g_bound_nd->mac[2], g_bound_nd->mac[3],
                 g_bound_nd->mac[4], g_bound_nd->mac[5]);
    } else {
        memset(nif->hwaddr, 0, 6);
        nif->hwaddr_len = 6;
    }
    return ERR_OK;
}
