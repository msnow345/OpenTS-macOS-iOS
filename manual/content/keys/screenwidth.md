---
key: ScreenWidth
summary: The width in pixels of the game screen.
---

This is the resolution the game renders at, not the size the picture is shown at. Raising it does not enlarge anything: it gives the tactical view more cells and the sidebar more room, while the front-end artwork keeps its own pixel size and sits in the middle of a larger, mostly empty screen. The menus, the score screens and the dialog panels are all drawn at 640 by 400 and none of them is scaled up to a larger screen.

To fill a larger window with the game as it was drawn, leave this and [`ScreenHeight`](/keys/screenheight/) at 640 by 400 and set [`WindowWidth`](/keys/windowwidth/) and [`WindowHeight`](/keys/windowheight/) to the size wanted; the picture is then scaled to fit. A full-screen game already does this, because it covers the desktop and scales the picture into it.
