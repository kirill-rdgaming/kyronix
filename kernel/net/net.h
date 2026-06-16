#pragma once
#include <stdbool.h>
#include <stdint.h>

// static net cfg for now
#define NET_MY_IP 0x0A00020Fu /* 10.0.2.15  */
#define NET_MASK 0xFFFFFF00u  /* /24         */
#define NET_GW 0x0A000202u    /* 10.0.2.2   */

void net_init(void);
void net_poll(void);
void net_receive(const uint8_t *eth_frame, uint16_t len);
