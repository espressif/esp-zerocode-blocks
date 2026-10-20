#include <esp_matter.h>
/* Cluster/attribute Id constants — esp_matter.h does not include the generated *_ids.h. */
#include <smoke_co_alarm_ids.h>
/* SmokeCoAlarm is a CODE-DRIVEN cluster in esp_matter 1.6: reads are served by
 * CHIP's SmokeCoAlarmCluster object, not esp_matter's attribute store. */
#include <app/clusters/smoke-co-alarm-server/SmokeCoAlarmCluster.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
