#include <stdint.h>
#include <stdbool.h>

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

#include "../../drivers/virtio_net.h"
#include "../../lib/string.h"
#include "../../lib/log.h"
#include "../../mm/heap.h"

/* called from irq/ poll context: hand a raw ethernet frame to lwip */
void kyronix_netif_input(struct netif* nif, const uint8_t* data, uint16_t len)
{
    struct pbuf* p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!p) return;

    /* copy frame into pbuf chain */
    struct pbuf* q = p;
    const uint8_t* src = data;
    uint16_t rem = len;
    while (q && rem) {
        uint16_t chunk = (rem < q->len) ? rem : (uint16_t)q->len;
        memcpy(q->payload, src, chunk);
        src += chunk;
        rem -= chunk;
        q = q->next;
    }

    if (nif->input(p, nif) != ERR_OK)
        pbuf_free(p);
}

/* called by lwip when it wants to send a frame */
static err_t kyronix_netif_output(struct netif* nif, struct pbuf* p)
{
    (void)nif;

    /* gather pbuf chain into a flat buffer */
    uint8_t buf[1514];
    uint16_t total = 0;
    for (struct pbuf* q = p; q; q = q->next) {
        if (total + q->len > sizeof(buf)) return ERR_MEM;
        memcpy(buf + total, q->payload, q->len);
        total += (uint16_t)q->len;
    }

    virtnet_send(buf, total);
    return ERR_OK;
}

/* netif init callback */
err_t kyronix_netif_init(struct netif* nif)
{
    nif->name[0] = 'e';
    nif->name[1] = '0';
    nif->output     = etharp_output;    /* arp+ip */
    nif->linkoutput = kyronix_netif_output;
    nif->mtu        = 1500;
    nif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;

    const uint8_t* mac = virtnet_mac();
    memcpy(nif->hwaddr, mac, 6);
    nif->hwaddr_len = 6;

    log_info("net: kyronix netif init, MAC %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ERR_OK;
}
