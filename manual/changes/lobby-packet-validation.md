---
title: Check a lobby packet before acting on it
category: fix
release: 0.2.0
targets:
- type: system
  id: network-packet-validation
  effect: changed
credit: [OpenTS contributors]
---

A global packet that arrives while the network lobby is up is checked for a whole packet and for a terminator on every fixed wire string its handlers read, which is what an in-game packet already got. A packet that fails is counted and dropped. The lobby also refuses a join whose house or color falls outside the tables they index, ignores a game-options string longer than the field it is written into, and keeps a scenario download inside the buffer it was given. Before, a peer could make the lobby read and write past those fields.
