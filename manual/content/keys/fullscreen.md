---
key: Fullscreen
summary: Whether the game covers the whole screen instead of running in a resizable window.
when_omitted:
  kind: value
  value: "yes"
---

A full-screen game opens a borderless window the size of the desktop. A windowed game opens an ordinary framed window that can be moved, resized, and maximized. Neither one changes the desktop's own resolution: the game always renders at [`ScreenWidth`](/keys/screenwidth/) by [`ScreenHeight`](/keys/screenheight/) and that picture is scaled into whichever window it has, so alt-tabbing away and back does not disturb the rest of the desktop.

This setting is read before the window is created, well before the rest of `SUN.INI`, and it is written back whenever the game saves its options. The display options screen offers it as a check box beside the resolution list and moves the window as soon as the screen is accepted, without the trial and rollback a resolution gets: the frame keeps the resolution it had and the player can see straight away whether the screen is covered.

The [`-WIN`](/using/command-line/windowed/) command line option asks for a window regardless of what this setting says. It applies to that run only and is never written back, so a launcher can offer a window without disturbing the player's own preference.

[`WindowWidth`](/keys/windowwidth/) and [`WindowHeight`](/keys/windowheight/) size the window when this setting is off. They are ignored while the game is full screen.
