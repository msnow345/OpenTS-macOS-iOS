---
key: LegacyDialogs
summary: Returns the rebuilt screens to the dialogs they replaced.
when_omitted:
  kind: value
  value: "no"
---

The game's dialogs are being rebuilt one screen at a time. A screen that has been rebuilt keeps the dialog it replaced alongside it, and `LegacyDialogs=yes` is what selects the old one. Set it when a rebuilt screen misbehaves, so that the screen can still be reached while the fault is reported.

The choice is read once per screen, as the screen opens, so a running screen is never swapped for the other one. Screens that have not been rebuilt are unaffected either way, and a rebuilt screen whose files cannot be loaded falls back to its dialog on its own without the key being set.

The key exists only while both halves do. It is written back to `sun.ini` with the rest of `[Options]`, and it goes when the last dialog does.
