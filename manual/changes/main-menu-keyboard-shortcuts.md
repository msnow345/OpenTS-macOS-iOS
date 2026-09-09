---
title: Retire the main menu's version and credits shortcuts
category: fix
release: 0.2.0
breaking: true
migration:
- Open the version screen from the main menu instead of pressing Ctrl+V.
- Start the credits from the main menu instead of pressing Ctrl+Alt+C. Escape still stops them.
targets:
- type: command
  id: fixed:main-menu-version
  effect: removed
- type: command
  id: fixed:main-menu-credits
  effect: removed
credit: [OpenTS contributors]
---

The classic main menu read the keyboard directly and opened the version dialog on Ctrl+V and
the credits on Ctrl+Alt+C. `Main_Menu` now shows the menu screen rather than polling for keys,
so neither shortcut has anywhere to be handled and both are gone.

Only the shortcuts were lost. The version screen and the credits are both still reached from
the menu itself, and Escape still stops the credits.
