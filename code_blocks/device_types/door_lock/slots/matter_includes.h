#include <esp_matter.h>
/* Cluster/attribute Id constants — esp_matter.h does not include the generated *_ids.h. */
#include <door_lock_ids.h>
/* DoorLockServer + the emberAfPluginDoorLockOn* hooks a remote Lock/Unlock calls. */
#include <app/clusters/door-lock-server/door-lock-server.h>
