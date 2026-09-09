#!/bin/bash
# Builds, stages, signs and installs the iOS application bundle in one command. The
# environment contract is in docs/BUILDING.md and in ios-signing.env.example.
#
# CMake's own iOS application target is the shipping bundle. Everything the game links on
# iOS is static, so there is no shell application and no inside-out re-signing:
#
#   configure -> xcodebuild (signs the bare app and mints or refreshes the profile)
#             -> cmake --install (stages the bundle, invalidating that signature)
#             -> stage the app icon
#             -> manifest check
#             -> codesign --force (re-seal the complete bundle)
#             -> devicectl install / copy / launch
#
# No team id, signing identity, bundle id or device identifier is committed. They come from
# the environment, or from the git-ignored file named by OPENTS_IOS_ENV_FILE.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
PRESET=ios-device-xcode
BUILD_DIR="$ROOT/build/$PRESET"
STAGE_DIR="$ROOT/build/ios-stage"
ICON_DIR="$ROOT/build/ios-icons"
DATA_DIR="$ROOT/build/ios-data"
APP="$STAGE_DIR/OpenTS.app"
# The target sets RUNTIME_OUTPUT_DIRECTORY, so Xcode writes the bundle under bin/<config>
# rather than into its own <config>-<platform> directory.
XCODE_APP="$BUILD_DIR/bin/Release/OpenTS.app"
PROJECT="$BUILD_DIR/OpenTS.xcodeproj"

usage() {
  cat <<'USAGE'
usage: scripts/build/ios/package-ios.sh [options]

  --clean            Delete the Xcode build tree and the stage first, then build it all.
  --resign-only      Skip the CMake configure and reuse the warm build and stage trees.
                     Still runs the incremental xcodebuild that refreshes the provisioning
                     profile, then re-stages, re-signs and, with --install, reinstalls.
  --no-build         Sign and stage whatever is already built. Does not refresh the
                     provisioning profile.
  --install          Install the signed bundle on the device.
  --launch           Launch it with the console attached.
  --device <id>      devicectl identifier. Overrides OPENTS_IOS_DEVICE.
  --push-data <dir>  Copy a Tiberian Sun installation into the app container at
                     Documents/OpenTS, leaving out the Windows executables, the editor and
                     the documentation, and adding the bundle's own ui tree and
                     Language.dat. About 2 GB, most of it the movie archives, which the
                     game refuses to start without.
  --touch-log        Create Documents/touchlog on the device, which is what turns the
                     touch recognizer's log on.
  -h, --help         This message.

Environment (see scripts/build/ios/ios-signing.env.example):
  OPENTS_IOS_TEAM_ID            required  Apple Developer team id (DEVELOPMENT_TEAM)
  OPENTS_IOS_CODESIGN_IDENTITY  required  full codesign identity string, quoted
  OPENTS_IOS_BUNDLE_ID          required  CFBundleIdentifier
  OPENTS_IOS_DEVICE             optional  devicectl identifier
  OPENTS_IOS_DEVICE_UDID        optional  hardware UDID for xcodebuild -destination
  OPENTS_IOS_BUNDLE_VERSION     optional  CFBundleVersion (default 1)
  OPENTS_IOS_ENV_FILE           optional  file sourced for the above; defaults to
                                          scripts/build/ios/ios-signing.env if present
USAGE
}

# ---------------------------------------------------------------- argument parsing
MODE=full
DO_INSTALL=0
DO_LAUNCH=0
DO_TOUCH_LOG=0
DEVICE_ARG=
PUSH_DATA=
while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean) MODE=clean ;;
    --resign-only) MODE=resign ;;
    --no-build) MODE=nobuild ;;
    --install) DO_INSTALL=1 ;;
    --launch) DO_LAUNCH=1 ;;
    --touch-log) DO_TOUCH_LOG=1 ;;
    --device) DEVICE_ARG=${2:?--device needs an identifier}; shift ;;
    --push-data) PUSH_DATA=${2:?--push-data needs a directory}; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "package-ios: unknown option '$1'" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

# ---------------------------------------------------------------- environment contract
# Values already exported in the caller's shell win over the file.
ENV_FILE=${OPENTS_IOS_ENV_FILE:-$ROOT/scripts/build/ios/ios-signing.env}
if [[ -f $ENV_FILE ]]; then
  saved=
  for v in OPENTS_IOS_TEAM_ID OPENTS_IOS_CODESIGN_IDENTITY OPENTS_IOS_BUNDLE_ID \
           OPENTS_IOS_DEVICE OPENTS_IOS_DEVICE_UDID OPENTS_IOS_BUNDLE_VERSION \
           OPENTS_IOS_ICON_SOURCE OPENTS_IOS_ICON_BG OPENTS_IOS_ICON_INSET; do
    eval "cur=\${$v:-}"
    [[ -n $cur ]] && saved="$saved $v=$(printf %q "$cur")"
  done
  # shellcheck disable=SC1090
  . "$ENV_FILE"
  [[ -n $saved ]] && eval "export $saved"
  echo "==> signing environment from $ENV_FILE"
fi

require() {
  eval "val=\${$1:-}"
  [[ -n $val ]] || { echo "package-ios: $1 is not set (see --help)" >&2; exit 1; }
}
require OPENTS_IOS_TEAM_ID
require OPENTS_IOS_CODESIGN_IDENTITY
require OPENTS_IOS_BUNDLE_ID

TEAM_ID=$OPENTS_IOS_TEAM_ID
IDENTITY=$OPENTS_IOS_CODESIGN_IDENTITY
BUNDLE_ID=$OPENTS_IOS_BUNDLE_ID
BUNDLE_VERSION=${OPENTS_IOS_BUNDLE_VERSION:-1}
DEVICE=${DEVICE_ARG:-${OPENTS_IOS_DEVICE:-}}
DEVICE_UDID=${OPENTS_IOS_DEVICE_UDID:-}

# A machine that has belonged to two teams holds more than one "Apple Development"
# certificate, and a prefix that matches several is fatal rather than ambiguous.
matches=$(security find-identity -v -p codesigning | grep -c -F "$IDENTITY" || true)
if [[ $matches -eq 0 ]]; then
  echo "package-ios: no codesigning identity matches '$IDENTITY'" >&2
  security find-identity -v -p codesigning >&2
  exit 1
elif [[ $matches -gt 1 ]]; then
  echo "package-ios: '$IDENTITY' matches $matches identities; give the full string" >&2
  security find-identity -v -p codesigning >&2
  exit 1
fi

# A Distribution certificate signs for the App Store and produces a bundle the device
# refuses to install directly. This is the error that costs the longest to recognize.
case "$IDENTITY" in
  *"Apple Distribution"*|*"iPhone Distribution"*)
    echo "package-ios: '$IDENTITY' is a distribution certificate, which cannot be" >&2
    echo "             installed on a device directly. Use an Apple Development one." >&2
    exit 1 ;;
esac

echo "==> team $TEAM_ID, bundle id $BUNDLE_ID"

# ---------------------------------------------------------------- configure
if [[ $MODE == clean ]]; then
  echo "==> clean: removing $BUILD_DIR and $STAGE_DIR"
  rm -rf "$BUILD_DIR" "$STAGE_DIR"
fi

if [[ $MODE == resign || $MODE == nobuild ]]; then
  [[ -d $BUILD_DIR ]] || {
    echo "package-ios: $BUILD_DIR does not exist; run without --resign-only first" >&2
    exit 1
  }
  cached=$(sed -n 's/^OPENTS_IOS_BUNDLE_ID:STRING=//p' "$BUILD_DIR/CMakeCache.txt")
  if [[ $cached != "$BUNDLE_ID" ]]; then
    echo "package-ios: the warm build tree is configured for bundle id '$cached', not" >&2
    echo "             '$BUNDLE_ID'. Re-run without --resign-only to reconfigure." >&2
    exit 1
  fi
else
  echo "==> cmake --preset $PRESET"
  cmake --preset "$PRESET" \
    -DOPENTS_IOS_BUNDLE_ID="$BUNDLE_ID" \
    -DOPENTS_IOS_BUNDLE_VERSION="$BUNDLE_VERSION" >/dev/null
fi

# ---------------------------------------------------------------- build and sign the app
if [[ $MODE != nobuild ]]; then
  echo "==> xcodebuild (signs the bare app and refreshes the provisioning profile)"
  dest=()
  if [[ -n $DEVICE_UDID ]]; then
    # A team with no registered devices cannot mint a profile until xcodebuild is told
    # which device to register, and -destination wants the hardware UDID rather than the
    # devicectl identifier.
    dest=(-destination "platform=iOS,id=$DEVICE_UDID")
  fi
  XCODEBUILD_LOG="$ROOT/build/ios-xcodebuild.log"
  set +e
  xcodebuild -project "$PROJECT" -target OpenTS -configuration Release \
    -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
    "${dest[@]+"${dest[@]}"}" \
    DEVELOPMENT_TEAM="$TEAM_ID" \
    CODE_SIGN_STYLE=Automatic \
    PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE_ID" > "$XCODEBUILD_LOG" 2>&1
  rc=$?
  set -e
  grep -E "Signing Identity|Provisioning Profile|BUILD (SUCCEEDED|FAILED)" "$XCODEBUILD_LOG" || true
  if [[ $rc -ne 0 ]]; then
    echo "package-ios: xcodebuild failed (exit $rc). Full log: $XCODEBUILD_LOG" >&2
    grep -E "error:" "$XCODEBUILD_LOG" | head -40 >&2
    exit $rc
  fi
  [[ -x $XCODE_APP/OpenTS ]] || { echo "package-ios: xcodebuild produced no binary" >&2; exit 1; }
fi

# ---------------------------------------------------------------- stage
if [[ $MODE == full || $MODE == clean ]]; then
  rm -rf "$STAGE_DIR"
fi
echo "==> cmake --install -> $STAGE_DIR"
cmake --install "$BUILD_DIR" --config Release --prefix "$STAGE_DIR" --component OpenTS >/dev/null

echo "==> app icon"
"$ROOT/scripts/build/ios/make-icons.sh" "$ICON_DIR"
cp "$ICON_DIR/Assets.car" "$APP/Assets.car"
cp "$ICON_DIR/AppIcon60x60@2x.png" "$ICON_DIR/AppIcon76x76@2x.png" \
   "$ICON_DIR/AppIcon83.5x83.5@2x.png" "$APP/"

# The profile Xcode embedded is what the entitlements below are checked against, and
# cmake --install does not carry it over.
cp "$XCODE_APP/embedded.mobileprovision" "$APP/embedded.mobileprovision"

# ---------------------------------------------------------------- manifest check
# Never install a bundle that is quietly missing part of the game.
missing=0
for required in OpenTS Info.plist embedded.mobileprovision Assets.car Language.dat ui \
                "AppIcon60x60@2x.png" "AppIcon76x76@2x.png" "AppIcon83.5x83.5@2x.png" \
                ui/mainmenu.rml ui/optionsbase.rcss; do
  if [[ ! -e $APP/$required ]]; then
    echo "package-ios: staged bundle is missing $required" >&2
    missing=1
  fi
done
[[ $missing -eq 0 ]] || exit 1

plist_id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$APP/Info.plist")
[[ $plist_id == "$BUNDLE_ID" ]] || {
  echo "package-ios: staged Info.plist says $plist_id, expected $BUNDLE_ID" >&2
  exit 1
}

# ---------------------------------------------------------------- sign
# Staging added files after Xcode signed, which invalidated its seal. The entitlements are
# taken from Xcode's own product rather than written by hand.
ENTITLEMENTS="$ROOT/build/ios-stage-entitlements.plist"
codesign -d --entitlements - --xml "$XCODE_APP" > "$ENTITLEMENTS" 2>/dev/null
echo "==> codesign"
codesign --force --sign "$IDENTITY" --entitlements "$ENTITLEMENTS" --timestamp=none "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

echo "==> $(find "$APP" -type f | wc -l | tr -d ' ') files, $(du -sh "$APP" | cut -f1) in $APP"

# ---------------------------------------------------------------- device
need_device() {
  [[ -n $DEVICE ]] || {
    echo "package-ios: no device; pass --device or set OPENTS_IOS_DEVICE" >&2
    exit 1
  }
}

# A device paired over Wi-Fi drops its tunnel when it locks, which fails a long transfer
# partway through. Retrying is normal operation rather than an error to abort on.
devicectl_retry() {
  local attempt=1
  while true; do
    if xcrun devicectl "$@"; then return 0; fi
    if [[ $attempt -ge 3 ]]; then
      echo "package-ios: devicectl $1 $2 failed after $attempt attempts" >&2
      echo "             (a locked device refuses a launch; unlock it and retry)" >&2
      return 1
    fi
    echo "    devicectl attempt $attempt failed; retrying in 5s" >&2
    attempt=$((attempt + 1))
    sleep 5
  done
}

if [[ $DO_INSTALL -eq 1 ]]; then
  need_device
  echo "==> devicectl install"
  devicectl_retry device install app --device "$DEVICE" "$APP"
fi

if [[ -n $PUSH_DATA ]]; then
  need_device
  [[ -d $PUSH_DATA ]] || { echo "package-ios: no such data directory: $PUSH_DATA" >&2; exit 1; }

  echo "==> staging game data -> $DATA_DIR"
  mkdir -p "$DATA_DIR"

  rsync -a --delete \
    --exclude 'Debug/' --exclude 'Saved Games/' \
    --exclude 'FinalSun/' --exclude 'Manuals/' --exclude 'HTML/' --exclude 'Internet/' \
    --exclude 'images/' \
    --exclude '*.exe' --exclude '*.EXE' --exclude '*.dll' --exclude '*.DLL' \
    --exclude '*.dylib' --exclude 'Game' --exclude 'GameD' \
    --exclude '*.pdb' --exclude '*.map' \
    --exclude 'SUN.INI' \
    "$PUSH_DATA/" "$DATA_DIR/"

  # The game reads its documents and its string table from the same directory it reads the
  # archives from, so the bundle's copies are pushed beside them.
  rsync -a --delete "$APP/ui/" "$DATA_DIR/ui/"
  cp "$APP/Language.dat" "$DATA_DIR/Language.dat"

  echo "==> pushing $(du -sh "$DATA_DIR" | cut -f1) into Documents/OpenTS"
  # --remove-existing-content wipes the whole container rather than the destination. Never.
  devicectl_retry device copy to --device "$DEVICE" \
    --domain-type appDataContainer --domain-identifier "$BUNDLE_ID" \
    --source "$DATA_DIR" --destination "Documents/OpenTS"
fi

if [[ $DO_TOUCH_LOG -eq 1 ]]; then
  need_device
  echo "==> creating Documents/touchlog"
  marker=$(mktemp -d)/touchlog
  mkdir -p "$marker"
  # An empty directory has no file node for devicectl to copy, so the folder carries a note
  # saying what it is for.
  cat > "$marker/README.txt" <<'NOTE'
The touch recognizer writes one log file into this folder for every run of the game.
Deleting the folder turns the logging off. Copy the files out with the Files app.
NOTE
  devicectl_retry device copy to --device "$DEVICE" \
    --domain-type appDataContainer --domain-identifier "$BUNDLE_ID" \
    --source "$marker" --destination "Documents/touchlog"
fi

if [[ $DO_LAUNCH -eq 1 ]]; then
  need_device
  echo "==> devicectl launch"
  devicectl_retry device process launch --console --terminate-existing \
    --device "$DEVICE" "$BUNDLE_ID"
fi
