#include <esp_matter.h>
/* Cluster/attribute Id constants — esp_matter.h does not include the generated *_ids.h. */
#include <soil_measurement_ids.h>
/* SoilMeasurement is code-driven (esp_matter 1.6): SetSoilMoistureLimits /
 * SetSoilMoistureMeasuredValue live in esp_matter's data_model_provider. */
#include <clusters/soil_measurement/integration.h>
