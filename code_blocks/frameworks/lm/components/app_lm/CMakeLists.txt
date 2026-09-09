# Only the SELECTED models are embedded. Both blobs ship in the catalogue, but
# a product that wants the 264 KB command parser must not also carry 2.0 MB of
# story weights — and the story model does not even fit the default partition
# table, so embedding it unconditionally would break every lm product.
#
# esp_tinylm binds a blob IN PLACE and reads the per-layer table straight from
# memory-mapped flash a few rows per token. That is why this is fast, and why
# the weights are EMBEDded in the app image rather than flashed to a partition.
set(zc_lm_srcs "app_lm.c")
set(zc_lm_embed "")

if(CONFIG_ZC_LM_MODEL_NLU)
    list(APPEND zc_lm_embed "nlu.bin")
endif()
if(CONFIG_ZC_LM_MODEL_STORY)
    list(APPEND zc_lm_srcs  "app_lm_story.c")
    list(APPEND zc_lm_embed "story.bin")
endif()

idf_component_register(
    SRCS ${zc_lm_srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES esp_tinylm
    EMBED_FILES ${zc_lm_embed}
)
