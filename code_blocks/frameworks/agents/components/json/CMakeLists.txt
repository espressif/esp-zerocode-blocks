# Shim, not a component of our own.
#
# ESP-IDF 6 removed the built-in `json` component; cJSON is now the managed
# component `espressif/cjson`. The upstream esp-agents-firmware `agent`
# component still declares `PRIV_REQUIRES json`, and there is nothing newer to
# bump to — its main branch HEAD IS the commit we pin (c5d6798, 2026-05-25).
#
# So rather than fork the SDK or drop the agents framework, this re-exports
# cJSON under the name the SDK asks for. REQUIRES (not PRIV_REQUIRES) on
# purpose: the point is to put cJSON's headers on the path of whatever requires
# `json`.
#
# cJSON is DEPRECATED upstream (2026-10: unmaintained, open security reports;
# the native espressif/json_parser + json_generator replace it everywhere else
# in this catalog). This shim is the LAST cJSON user we ship, and only because
# the SDK's own sources call cJSON_*; the fix is upstream, not here.
#
# Delete this the day esp-agents-firmware moves off cJSON (or declares it). It is
# scoped to the agents framework block, so it shadows nothing in a product that
# does not use agents.
idf_component_register(REQUIRES espressif__cjson)
