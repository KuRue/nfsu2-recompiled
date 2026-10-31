/* roam_switch.h -- the nfsu2.xyz switch transport under the portable
 * free-roam session.
 *
 * The free-roam hub and the browsers share the same /lan WebSocket as LAN
 * play, but free roam uses NET_KIND_UDP (kind 1) frames to 10.77.0.253:47210
 * and to peers.  This is the C side of that transport: the C++ runtime owns
 * the switch socket and calls roam_switch_receive for every kind-1 frame; the
 * session's io.send calls roam_switch_send.
 */
#ifndef ROAM_SWITCH_H
#define ROAM_SWITCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1 when the nfsu2.xyz provider is active (the switch, not the real LAN). */
int  roam_switch_active(void);
/* One datagram to a virtual address: source port `sport`, destination port
 * `dport`.  1 sent, 0 not (offline/full). */
int  roam_switch_send(uint32_t dst, uint16_t sport, uint16_t dport,
                      const uint8_t *data, size_t length);
/* One binary frame from the switch (16-byte switch header + datagram). */
void roam_switch_receive(const uint8_t *frame, size_t length);
/* The switch has no receive thread of its own: the game thread pumps the
 * session's clock (HELLOs to the hub, beacons, peer timeouts). */
void roam_switch_pump(void);

#ifdef __cplusplus
}
#endif
#endif
