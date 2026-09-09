#!/bin/bash
# Builds the iOS app icon set from the project's own icon artwork.
#
# Produces, under the output directory:
#   AppIcon.xcassets/AppIcon.appiconset/  every PNG the icon set declares
#   Assets.car                            the compiled catalog, selected by CFBundleIconName
#   AppIcon60x60@2x.png                   loose copies for the older icon mechanism, kept
#   AppIcon76x76@2x.png                   because SpringBoard serves a cached icon for a
#   AppIcon83.5x83.5@2x.png               developer signed install and honours these
#
# Everything here is a build product. Nothing is committed: the icons are regenerated
# whenever the artwork or the parameters change.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)

SOURCE=${OPENTS_IOS_ICON_SOURCE:-$ROOT/code/resources/app-icon/opents.svg}
OUT=${1:-$ROOT/build/ios-icons}
# iOS masks the icon itself and refuses an alpha channel, so the artwork is flattened onto
# an opaque colour. The artwork draws no frame of its own, so nothing is cropped off.
BG=${OPENTS_IOS_ICON_BG:-101010}
INSET=${OPENTS_IOS_ICON_INSET:-0.0}
MIN_OS=${OPENTS_IOS_MIN_OS:-16.0}

[[ -f $SOURCE ]] || { echo "make-icons: icon source not found: $SOURCE" >&2; exit 1; }

STAMP="$OUT/.stamp"
WANT="source=$SOURCE bg=$BG inset=$INSET mtime=$(stat -f %m "$SOURCE")"
if [[ -f $STAMP && -f $OUT/Assets.car ]] && [[ $(cat "$STAMP") == "$WANT" ]]; then
  echo "  icons: up to date ($OUT)"
  exit 0
fi

SET="$OUT/AppIcon.xcassets/AppIcon.appiconset"
rm -rf "$OUT"
mkdir -p "$SET"

# CoreGraphics reads no vector format, so a vector source is rasterized by Quick Look
# first. A raster source is composited straight from the file.
RASTER=$SOURCE
case "$SOURCE" in
  *.svg|*.SVG)
    RASTER="$OUT/source-1024.png"
    qlmanage -t -s 1024 -o "$OUT" "$SOURCE" >/dev/null 2>&1
    if [[ ! -f "$OUT/$(basename "$SOURCE").png" ]]; then
      echo "make-icons: Quick Look could not rasterize $SOURCE" >&2
      exit 1
    fi
    mv "$OUT/$(basename "$SOURCE").png" "$RASTER"
    ;;
esac

# The largest size is composited once and the rest scaled from it: the compositor costs a
# few seconds of swift compile per run and sips is immediate.
MASTER="$OUT/icon-1024.png"
swift "$ROOT/scripts/build/ios/composite-icon.swift" "$RASTER" "$MASTER" 1024 "$BG" "$INSET"

emit() { # emit <pixels> <filename>
  if [[ $1 -eq 1024 ]]; then cp "$MASTER" "$SET/$2"
  else sips -Z "$1" "$MASTER" --out "$SET/$2" >/dev/null; fi
}

# idiom, size, scale, pixel size, file name
ENTRIES="
iphone 20x20 2x 40 AppIcon20x20@2x.png
iphone 20x20 3x 60 AppIcon20x20@3x.png
iphone 29x29 2x 58 AppIcon29x29@2x.png
iphone 29x29 3x 87 AppIcon29x29@3x.png
iphone 40x40 2x 80 AppIcon40x40@2x.png
iphone 40x40 3x 120 AppIcon40x40@3x.png
iphone 60x60 2x 120 AppIcon60x60@2x.png
iphone 60x60 3x 180 AppIcon60x60@3x.png
ipad 20x20 1x 20 AppIcon20x20~ipad.png
ipad 20x20 2x 40 AppIcon20x20@2x~ipad.png
ipad 29x29 1x 29 AppIcon29x29~ipad.png
ipad 29x29 2x 58 AppIcon29x29@2x~ipad.png
ipad 40x40 1x 40 AppIcon40x40~ipad.png
ipad 40x40 2x 80 AppIcon40x40@2x~ipad.png
ipad 76x76 2x 152 AppIcon76x76@2x~ipad.png
ipad 83.5x83.5 2x 167 AppIcon83.5x83.5@2x~ipad.png
ios-marketing 1024x1024 1x 1024 AppIcon1024x1024.png
"

{
  echo '{'
  echo '  "images" : ['
  first=1
  while read -r idiom size scale px file; do
    [[ -z ${idiom:-} ]] && continue
    emit "$px" "$file"
    [[ $first -eq 1 ]] || echo '    },'
    first=0
    echo '    {'
    echo "      \"filename\" : \"$file\","
    echo "      \"idiom\" : \"$idiom\","
    echo "      \"scale\" : \"$scale\","
    echo "      \"size\" : \"$size\""
  done <<< "$ENTRIES"
  echo '    }'
  echo '  ],'
  echo '  "info" : { "author" : "xcode", "version" : 1 }'
  echo '}'
} > "$SET/Contents.json"

cat > "$OUT/AppIcon.xcassets/Contents.json" <<'JSON'
{
  "info" : { "author" : "xcode", "version" : 1 }
}
JSON

xcrun actool "$OUT/AppIcon.xcassets" \
  --compile "$OUT" \
  --app-icon AppIcon \
  --output-partial-info-plist "$OUT/AppIcon-partial.plist" \
  --platform iphoneos \
  --minimum-deployment-target "$MIN_OS" \
  --target-device iphone --target-device ipad \
  --output-format human-readable-text >/dev/null

[[ -f $OUT/Assets.car ]] || { echo "make-icons: actool produced no Assets.car" >&2; exit 1; }

cp "$SET/AppIcon60x60@2x.png"          "$OUT/AppIcon60x60@2x.png"
cp "$SET/AppIcon76x76@2x~ipad.png"     "$OUT/AppIcon76x76@2x.png"
cp "$SET/AppIcon83.5x83.5@2x~ipad.png" "$OUT/AppIcon83.5x83.5@2x.png"

for f in "$OUT"/AppIcon*.png; do
  if sips -g hasAlpha "$f" | grep -q "hasAlpha: yes"; then
    echo "make-icons: $f still carries an alpha channel, which iOS rejects" >&2
    exit 1
  fi
done

echo "$WANT" > "$STAMP"
echo "  icons: generated from $(basename "$SOURCE") -> $OUT"
