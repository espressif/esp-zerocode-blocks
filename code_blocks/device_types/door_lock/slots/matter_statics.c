static uint16_t s_{{prefix_lc}}_endpoint_id = 0;

#ifndef ZC_DOOR_LOCK_REMOTE_OPS
#define ZC_DOOR_LOCK_REMOTE_OPS
/* A remote LockDoor / UnlockDoor is NOT an attribute write: CHIP's door-lock
 * server checks the credentials, then asks the application through these two
 * global hooks. Their weak defaults answer "failed" without touching
 * LockState, so without them every Lock/Unlock from a controller fails.
 * They are global symbols, so the first door_lock instance defines them once
 * and every instance registers its endpoint below. On success they set
 * LockState; the LockState hook in matter_attr_cb then drives the lock. */
static uint16_t s_zc_door_lock_eps[8];
static int s_zc_door_lock_count = 0;

static void zc_door_lock_register(uint16_t ep)
{
    if (s_zc_door_lock_count < (int)(sizeof(s_zc_door_lock_eps) / sizeof(s_zc_door_lock_eps[0])))
        s_zc_door_lock_eps[s_zc_door_lock_count++] = ep;
}

static bool zc_door_lock_remote(chip::EndpointId ep, DlLockState state,
                                chip::app::Clusters::DoorLock::OperationErrorEnum &err)
{
    for (int i = 0; i < s_zc_door_lock_count; i++) {
        if (s_zc_door_lock_eps[i] == ep) {
            if (DoorLockServer::Instance().SetLockState(ep, state)) return true;
            break;
        }
    }
    err = chip::app::Clusters::DoorLock::OperationErrorEnum::kUnspecified;
    return false;
}

bool emberAfPluginDoorLockOnDoorLockCommand(chip::EndpointId endpointId,
    const chip::app::DataModel::Nullable<chip::FabricIndex> &fabricIdx,
    const chip::app::DataModel::Nullable<chip::NodeId> &nodeId,
    const chip::Optional<chip::ByteSpan> &pinCode,
    chip::app::Clusters::DoorLock::OperationErrorEnum &err)
{
    return zc_door_lock_remote(endpointId, DlLockState::kLocked, err);
}

bool emberAfPluginDoorLockOnDoorUnlockCommand(chip::EndpointId endpointId,
    const chip::app::DataModel::Nullable<chip::FabricIndex> &fabricIdx,
    const chip::app::DataModel::Nullable<chip::NodeId> &nodeId,
    const chip::Optional<chip::ByteSpan> &pinCode,
    chip::app::Clusters::DoorLock::OperationErrorEnum &err)
{
    return zc_door_lock_remote(endpointId, DlLockState::kUnlocked, err);
}

/* AutoRelockTime, when a controller sets one, ends here; the weak default
 * does nothing and the door would stay unlocked. */
void emberAfPluginDoorLockOnAutoRelock(chip::EndpointId endpointId)
{
    chip::app::Clusters::DoorLock::OperationErrorEnum err;
    (void)zc_door_lock_remote(endpointId, DlLockState::kLocked, err);
}
#endif
