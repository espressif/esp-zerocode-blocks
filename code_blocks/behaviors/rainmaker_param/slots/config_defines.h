#define {{prefix}}_RMP_KIND_{{cfg.kind}}      1
#define {{prefix}}_RMP_VT_{{cfg.value_kind}}  1
/* >1: the param holds value x scale (hundredths of a degree = 100); the app
   shows and writes the real value as a decimal. Numbers only. */
#define {{prefix}}_RMP_SCALE {{cfg.scale}}
#if {{prefix}}_RMP_SCALE > 1 && !defined({{prefix}}_RMP_KIND_toggle) && !defined({{prefix}}_RMP_KIND_time) && !defined({{prefix}}_RMP_VT_bool)
#define {{prefix}}_RMP_SCALED 1
#endif
