---
title: Keep saved games in a file of the engine's own
category: feature
release: 0.2.0
targets:
- type: format
  id: save-games
  effect: changed
credit:
- Gunnar Beutner
---

A saved game is now a file of the engine's own format rather than an OLE compound document: a header, the listing details, and the game state as one compressed block, written under a temporary name and moved into place once complete. The load dialog reads only the header of each file, and a damaged or truncated file is refused before the running game is disturbed. Saved games written before this change are not read and no longer appear in the load dialog; there is no conversion.
