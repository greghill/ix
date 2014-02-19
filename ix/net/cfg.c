/*
 * cfg.c - network stack configuration parameters
 *
 * FIXME: this is really terrible right now. We need a proper parser and
 * loader to pull in the network configuration from a file. That code
 * should eventually go here.
 *
 * FIXME: we're hardcoded to a single gateway.
 *
 * FIXME: we're hardcoded to a single interface.
 */

#include <ix/types.h>
#include <net/ip.h>
#include <net/ethernet.h>

struct ip_addr cfg_host_addr;
struct ip_addr cfg_broadcast_addr = {MAKE_IP_ADDR(10, 79, 6, 255)};
struct ip_addr cfg_gateway_addr = {MAKE_IP_ADDR(10, 79, 6, 0)};
uint32_t cfg_mask = MAKE_IP_ADDR(255, 255, 255, 0);
struct eth_addr cfg_mac;
