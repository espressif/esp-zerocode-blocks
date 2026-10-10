# GCC 16 added -Wnon-c-typedef-for-linkage, and esp-dl (pulled in by esp-sr)
# trips it in a managed component, so the diagnostic is demoted rather than fixed.
add_compile_options(-Wno-error=non-c-typedef-for-linkage)
