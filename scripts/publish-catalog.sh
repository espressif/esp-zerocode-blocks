#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Build — and by default publish — a VERSIONED firmware catalog bundle, the
# artifact the platform fetches at apply-time instead of baking blocks into its
# image. Bundle = assembled code_blocks + product_configurations +
# base_firmware, tarred, versioned by content hash. A pin resolves to a
# specific version; manifest.json names the current one.
#
#   scripts/publish-catalog.sh --stage-only <dir>
#       Build only, no AWS: writes <dir>/<version>.tar.gz, <dir>/manifest.json,
#       <dir>/library.json and <dir>/VERSION. What a ZeroCode AI release runs;
#       its deploy uploads exactly these files.
#
#   CATALOG_BUCKET=<bucket> [CATALOG_BASE_URL=<site>] scripts/publish-catalog.sh
#       Build and upload to a deployment's catalog bucket (maintainers only;
#       needs AWS credentials for it).
set -euo pipefail
cd "$(dirname "$0")/.."

PREFIX="firmware-catalog"
STAGE_ONLY=""
if [ "${1:-}" = "--stage-only" ]; then
  STAGE_ONLY="${2:?--stage-only <dir>}"
elif [ $# -gt 0 ]; then
  echo "usage: $0 [--stage-only <dir>]" >&2; exit 2
else
  bucket="${CATALOG_BUCKET:?set CATALOG_BUCKET to the deployment catalog bucket}"
fi

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
stage="$work/catalog"
mkdir -p "$stage"
python3 scripts/assemble_blocks.py --out "$stage/code_blocks" --products-out "$stage/product_configurations" >/dev/null
rsync -a --exclude build --exclude managed_components --exclude sdkconfig base_firmware "$stage/"

# content-addressed version: hash of the sorted file contents (stable, no timestamps)
version=$(cd "$stage" && find . -type f -exec sh -c 'if command -v sha256sum >/dev/null; then sha256sum "$1"; else shasum -a 256 "$1"; fi' _ {} \; | sort | sha256 | cut -c1-16)
[[ "$version" =~ ^[0-9a-f]{16}$ ]] || { echo "no catalog version computed" >&2; exit 1; }
tar="$PREFIX/$version.tar.gz"
tar -czf "$work/bundle.tgz" -C "$work" catalog
printf '{\n  "version": "%s",\n  "tarball": "%s",\n  "blocks": %s,\n  "products": %s\n}\n' \
  "$version" "$tar" \
  "$(find code_blocks -name block.yml | wc -l | tr -d ' ')" \
  "$(find product_configurations -name product.yml | wc -l | tr -d ' ')" \
  > "$work/manifest.json"

# Browsable listing (block + template metadata) — symmetric to esp-virtual-parts'
# hardware/index.json. ZeroCode AI's public Library fetches this at runtime, so a
# new block/template shows up WITHOUT a UI deploy. Not versioned: it's display
# metadata, always current (the tarball above is the pinned, reproducible one).
python3 scripts/build_library_index.py --out "$work/library.json" >/dev/null
echo "version $version"

if [ -n "$STAGE_ONLY" ]; then
  mkdir -p "$STAGE_ONLY"
  cp "$work/bundle.tgz" "$STAGE_ONLY/$version.tar.gz"
  cp "$work/manifest.json" "$work/library.json" "$STAGE_ONLY/"
  echo "$version" > "$STAGE_ONLY/VERSION"
  echo "staged → $STAGE_ONLY (nothing uploaded)"
  exit 0
fi

aws s3 cp "$work/bundle.tgz" "s3://$bucket/$tar" \
  --cache-control "public,max-age=31536000,immutable" --content-type application/gzip
aws s3 cp "$work/manifest.json" "s3://$bucket/$PREFIX/manifest.json" \
  --cache-control "no-cache" --content-type application/json
aws s3 cp "$work/library.json" "s3://$bucket/$PREFIX/library.json" \
  --cache-control "no-cache" --content-type application/json

# No CloudFront invalidation: the serving distribution routes
# /firmware-catalog/* with CachingDisabled, so the upload is live immediately.
base="${CATALOG_BASE_URL:-https://esp-zerocode.ai}"
echo "published → $base/$PREFIX/manifest.json"
# Counted in its own step: nesting a double-quoted python one-liner inside
# $(...) inside a double-quoted echo is a parse error on bash 5.3.
counts=$(python3 -c "import json,sys;d=json.load(open(sys.argv[1]));print(len(d['blocks']),'blocks,',len(d['templates']),'templates')" "$work/library.json")
echo "library   → $base/$PREFIX/library.json  ($counts)"
echo "(pin version: $version)"
