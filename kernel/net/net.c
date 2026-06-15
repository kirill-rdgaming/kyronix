#include "net.h"
#include "../drivers/virtio_net.h"
#include "../lib/log.h"
#include "../mm/heap.h"
#include "../arch/x86_64/pit.h"

#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "lwip/dns.h"
#include "netif/ethernet.h"

err_t  kyronix_netif_init(struct netif* nif);
void   kyronix_netif_input(struct netif* nif, const uint8_t* data, uint16_t len);

static struct netif g_netif;

void net_init(void)
{
    if (!virtnet_ready()) {
        log_warn("net: virtio-net not ready, skipping lwIP init");
        return;
    }

    lwip_init();

    ip4_addr_t ip, mask, gw;
    IP4_ADDR(&ip,   10, 0, 2, 15);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw,   10, 0, 2, 2);

    netif_add(&g_netif, &ip, &mask, &gw, NULL, kyronix_netif_init, ethernet_input);
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    ip4_addr_t dns1;
    IP4_ADDR(&dns1, 10, 0, 2, 3);
    dns_init();
    dns_setserver(0, &dns1);

    log_info("net: lwIP initialized, IP 10.0.2.15/24 gw 10.0.2.2 dns 10.0.2.3");
}

void net_receive(const uint8_t* eth_frame, uint16_t len)
{
    kyronix_netif_input(&g_netif, eth_frame, len);
}

void net_poll(void)
{
    virtnet_poll(); /* drain rx -> works even if irq doesnt fire on q35 */
    static uint8_t s_ctr;
    if (++s_ctr == 0)
        sys_check_timeouts();
}
