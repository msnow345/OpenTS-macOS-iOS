---
title: Hear a peer on the same machine
category: fix
release: 0.2.0
targets:
- type: system
  id: network-packet-validation
  effect: changed
credit: [OpenTS contributors]
---

A datagram is discarded as this machine's own broadcast coming back only when its source port is the one this game is listening on, as well as its source address being one of this machine's. Before, the address alone was enough, so two copies of the game running on one machine discarded everything the other sent and never found each other.
