#include <esp_matter.h>
/* Cluster/attribute Id constants — esp_matter.h does not include the generated *_ids.h. */
#include <air_quality_ids.h>
/* CO2 / TVOC concentration clusters are code-driven in esp_matter 1.6 and
 * esp_matter registers no server for them — this block does (matter_statics). */
#include <app/clusters/concentration-measurement-server/ConcentrationMeasurementCluster.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <map>
