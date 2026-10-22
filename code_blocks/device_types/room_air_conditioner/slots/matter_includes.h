#include <esp_matter.h>
/* Cluster/attribute Id constants — esp_matter.h does not include the generated *_ids.h. */
#include <on_off_ids.h>
#include <thermostat_ids.h>
#include <fan_control_ids.h>
/* FanControl is a CODE-DRIVEN cluster in esp_matter 1.6 — the binding goes
 * through the cluster's delegate, see zc_matter_fan.h. */
#include "zc_matter_fan.h"
