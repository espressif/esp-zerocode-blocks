#include <soc/soc_caps.h>
#if !SOC_PCNT_SUPPORTED
#error "drivers/hlw8012 counts the meter's pulses with a PCNT unit, and this chip has none (esp32c2/c3/c61): use a chip with PCNT, or drivers/ina219 / drivers/ct_clamp_adc for metering"
#endif
#include <driver/pulse_cnt.h>
