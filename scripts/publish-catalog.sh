#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Publish a VERSIONED firmware catalog bundle to the CDN — the artifact the
# platform fetches at apply-time instead of baking blocks into its image.
# Bundle = assembled code_blocks + product_configurations + base_firmware,
# tarred, versioned by content hash. A pin resolves to a specific version;
# manifest.json names the current one.
#
#   CATALOG_BUCKET=<bucket> [CATALOG_BASE_URL=<site>] scripts/publish-catalog.sh
#
# Maintainers only. Publishes to a deployment's catalog bucket (the platform's
# `catalog_bucket` terraform output), served same-origin by that deployment's
# own distribution; needs AWS credentials for it.
set -euo pipefail
cd "$(dirname "$0")/.."

bucket="${CATALOG_BUCKET:?set CATALOG_BUCKET to the deployment catalog bucket}"
PREFIX="firmware-catalog"

work=$(mktemp -d)
stage="$work/catalog"
mkdir -p "$stage"
python3 scripts/assemble_blocks.py --out "$stage/code_blocks" --products-out "$stage/product_configurations" >/dev/null
rsync -a --exclude build --exclude managed_components --exclude sdkconfig base_firmware "$stage/"

# content-addressed version: hash of the sorted file contents (stable, no timestamps)
version=$(cd "$stage" && find . -type f -exec shasum -a 256 {} \; | sort | shasum -a 256 | cut -c1-16)
tar="$PREFIX/$version.tar.gz"
tar -czf "$work/bundle.tgz" -C "$work" catalog

echo "version $version"
aws s3 cp "$work/bundle.tgz" "s3://$bucket/$tar" \
  --cache-control "public,max-age=31536000,immutable" --content-type application/gzip
printf '{\n  "version": "%s",\n  "tarball": "%s",\n  "blocks": %s,\n  "products": %s\n}\n' \
  "$version" "$tar" \
  "$(find code_blocks -name block.yml | wc -l | tr -d ' ')" \
  "$(find product_configurations -name product.yml | wc -l | tr -d ' ')" \
  > "$work/manifest.json"
aws s3 cp "$work/manifest.json" "s3://$bucket/$PREFIX/manifest.json" \
  --cache-control "no-cache" --content-type application/json

# Browsable listing (block + template metadata) — symmetric to esp-virtual-parts'
# hardware/index.json. ZeroCode AI's public Library fetches this at runtime, so a
# new block/template shows up WITHOUT a UI deploy. Not versioned: it's display
# metadata, always current (the tarball above is the pinned, reproducible one).
python3 scripts/build_library_index.py --out "$work/library.json"
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
