#include <esp_matter.h>
/* Cluster/attribute Id constants — esp_matter.h does not include the generated *_ids.h. */
#include <electrical_energy_measurement_ids.h>
#include <electrical_power_measurement_ids.h>
#include <power_topology_ids.h>
#include <app/reporting/reporting.h>           /* MatterReportingAttributeChangeCallback */
#include <app/clusters/electrical-power-measurement-server/electrical-power-measurement-server.h>
