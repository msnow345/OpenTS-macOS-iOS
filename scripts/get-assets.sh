#!/bin/bash
# Download Tiberian Sun + Firestorm game data from your own Steam account into Run/.
#
# OpenTS supplies the engine, not the game data. This fetches the data files from
# a copy of the game you already own and places them where the engine looks for
# them, which the manual describes under "Game data".
#
# Usage: ./scripts/get-assets.sh <your_steam_username>
# Steam Guard: you will be prompted for the code on first login.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <steam_username>" >&2
    exit 1
fi

STEAM_USER="$1"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${TS_RUN_DIR:-$REPO_ROOT/Run}"
TMP_DIR="$REPO_ROOT/build/.steamcmd_ts"

# App 2229880 = "Command & Conquer Tiberian Sun and Firestorm". The depot is
# Windows-only, which does not matter: the archives this copies are data, and the
# Windows executables are excluded below because OpenTS replaces them.
STEAM_APP_ID=2229880

if ! command -v steamcmd >/dev/null 2>&1; then
    echo "Error: steamcmd is not installed." >&2
    echo "  macOS:  brew install --cask steamcmd" >&2
    echo "  Linux:  install the steamcmd package for your distribution" >&2
    exit 1
fi

mkdir -p "$TMP_DIR" "$DEST"

# macOS Gatekeeper quarantines steamcmd's unnotarized bundled frameworks when
# Homebrew installs the cask. On first run that pops a blocking "Apple could not
# verify ... malware" dialog which stops steamcmd dead. Clear the flag up front.
if [[ "$(uname)" == "Darwin" ]] && command -v brew >/dev/null 2>&1; then
    STEAMCMD_CASK="$(brew --prefix)/Caskroom/steamcmd"
    [[ -d "$STEAMCMD_CASK" ]] && xattr -dr com.apple.quarantine "$STEAMCMD_CASK" 2>/dev/null || true
fi

# steamcmd and the Steam desktop client share one data directory. A running
# client holds a single-instance lock, so steamcmd stalls forever right after
# "Verifying installation..." with no error at all. Fail fast and say why.
#
# Match the client binary itself, not anything living under the Steam bundle.
# Quitting Steam leaves helpers such as ipcserver and steamwebhelper running for
# a while, and they hold no lock — a broader pattern reports the client as
# running when it is not, and no amount of quitting Steam clears it.
if pgrep -x steam_osx >/dev/null 2>&1 \
    || pgrep -x steam >/dev/null 2>&1; then
    echo "Error: the Steam desktop client is running." >&2
    echo "steamcmd shares Steam's data directory, and the running client locks it —" >&2
    echo 'steamcmd would hang forever after "Verifying installation...".' >&2
    echo "Quit Steam completely (Steam > Quit Steam, or Cmd-Q), then re-run this script." >&2
    exit 1
fi

echo "==> Downloading app $STEAM_APP_ID from Steam as '$STEAM_USER'"
steamcmd \
    +@sSteamCmdForcePlatformType windows \
    +force_install_dir "$TMP_DIR" \
    +login "$STEAM_USER" \
    +app_update "$STEAM_APP_ID" validate \
    +quit

echo "==> Copying game data into $DEST"
# Data only. OpenTS replaces the game's own executables, and copying them in
# would leave two things called Game in the same directory.
rsync -a \
    --exclude="*.exe" --exclude="*.dll" --exclude="*.pdb" \
    --exclude="_CommonRedist/" --exclude="installscript.vdf" \
    "$TMP_DIR/" "$DEST/"

# The manual's MIX archive page lists what startup actually requires: CACHE.MIX
# first, then CONQUER.MIX, SOUNDS.MIX, SCORES.MIX, a movie archive, and
# SOUNDS01.MIX where the expansion is present. Everything else is mounted when
# found and passed over when not, so check only what the engine refuses to start
# without, and check it case-insensitively because the depot's casing varies.
echo "==> Verifying the archives startup requires"
missing=0
for archive in CACHE.MIX TIBSUN.MIX LOCAL.MIX CONQUER.MIX SOUNDS.MIX SCORES.MIX; do
    if ! find "$DEST" -maxdepth 2 -iname "$archive" -print -quit | grep -q .; then
        echo "  MISSING: $archive" >&2
        missing=1
    else
        echo "  found:   $archive"
    fi
done

# A movie archive is required, but its name varies by release and by disc.
if find "$DEST" -maxdepth 2 -iname "MOVIES*.MIX" -print -quit | grep -q .; then
    echo "  found:   $(find "$DEST" -maxdepth 2 -iname 'MOVIES*.MIX' -exec basename {} \; | tr '\n' ' ')"
else
    echo "  MISSING: a MOVIES*.MIX archive" >&2
    missing=1
fi

# Firestorm's speech archive is required only where Firestorm is installed,
# which FIRESTRM.INI identifies.
if find "$DEST" -maxdepth 2 -iname "FIRESTRM.INI" -print -quit | grep -q .; then
    if find "$DEST" -maxdepth 2 -iname "SOUNDS01.MIX" -print -quit | grep -q .; then
        echo "  found:   SOUNDS01.MIX (Firestorm)"
    else
        echo "  MISSING: SOUNDS01.MIX, and FIRESTRM.INI says Firestorm is installed" >&2
        missing=1
    fi
fi

if [[ $missing -ne 0 ]]; then
    echo >&2
    echo "Error: the download completed but the archives the engine needs are not present." >&2
    echo "Check that the Steam account owns Tiberian Sun and that app $STEAM_APP_ID installed cleanly." >&2
    exit 1
fi

echo
echo "Done. Game data is in $DEST"
echo "Run the engine with: $DEST/Game"
