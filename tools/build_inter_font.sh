#!/usr/bin/env bash
# Regenerate src/font_inter_black_92.h from Inter-Black.otf
#
# Pipeline: Inter-Black.otf -> otf2bdf (92pt @ 72dpi) -> U8g2's bdfconv -> .h header
# Glyph map: 0x30-0x3A (digits 0-9 plus colon ':' for the clock view)
#
# Required tools:
#   - otf2bdf:  brew install otf2bdf
#   - bdfconv:  built from u8g2 source. Set BDFCONV env var to its path,
#               or run from a checkout that has it on PATH.
#
# Inter source: official v4.0 release at
#   https://github.com/rsms/inter/releases/download/v4.0/Inter-4.0.zip

set -euo pipefail

cd "$(dirname "$0")/.."
OUT_HEADER="src/font_inter_black_92.h"
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

OTF="${INTER_OTF:-$TMP/Inter-Black.otf}"
if [ ! -f "$OTF" ]; then
  echo "Downloading Inter v4.0..."
  curl -sL "https://github.com/rsms/inter/releases/download/v4.0/Inter-4.0.zip" -o "$TMP/Inter.zip"
  unzip -p "$TMP/Inter.zip" "*/Inter-Black.otf" > "$OTF"
fi

BDF="$TMP/inter_black_92.bdf"
echo "Rasterising at 92pt..."
otf2bdf -p 92 -r 72 -o "$BDF" "$OTF"

# Default to the bdfconv binary checked into ./tools/
BDFCONV="${BDFCONV:-$(pwd)/tools/bdfconv}"
if ! command -v "$BDFCONV" >/dev/null 2>&1 && [ ! -x "$BDFCONV" ]; then
  echo "Error: bdfconv not found. Set BDFCONV=path or rebuild from u8g2/tools/font/bdfconv." >&2
  exit 1
fi

echo "Packing into U8g2 header..."
"$BDFCONV" -f 1 -m '48-58' -n font_inter_black_92 -o "$OUT_HEADER" "$BDF"

echo
echo "Wrote $OUT_HEADER"
ls -la "$OUT_HEADER"
