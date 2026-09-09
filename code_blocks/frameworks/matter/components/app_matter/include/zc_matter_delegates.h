/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Shared Matter cluster delegates — reusable across device_type blocks.
 *
 * A delegate cluster (OperationalState, ModeBase, …) is ~90% identical between
 * device types; only "which driver param" and the mode/state list differ. The
 * reusable class lives here ONCE; each device_type block instantiates it with
 * its own param (see device_types/dish_washer for the wiring).
 *
 * Header-only on purpose: it is #included from a block's matter_includes slot
 * (which lands at the top of app_matter.cpp, before the shell's s_handle), so
 * nothing here may reference s_handle. Commands write the param with
 * APP_DRIVER_SOURCE_LOCAL, which means the matter solution's driver_cb DOES
 * fire and becomes the single place that reflects param -> cluster — both for
 * a controller command and for a local (button/cloud) change.
 */
#pragma once

#include <esp_matter.h>
#include <esp_matter_data_model.h>
#include <app_driver.h>
#include <esp_log.h>
#include <app/clusters/operational-state-server/operational-state-server.h>
#include <app/clusters/mode-base-server/mode-base-server.h>
#include <app/clusters/closure-control-server/ClosureControlClusterDelegate.h>
#include <app/clusters/closure-dimension-server/closure-dimension-delegate.h>
#include <app/clusters/chime-server/chime-server.h>
#include <lib/support/Span.h>
#include <platform/CHIPDeviceLayer.h>
#include <system/SystemClock.h>

namespace zc {

/* ── OperationalState ──────────────────────────────────────────────────────
 * Command-driven: the cluster calls Start/Stop/Pause/Resume on the Matter
 * thread; each maps to the operational-state param (0 stopped, 1 running,
 * 2 paused, 3 error). Advertises the four standard states and no phases.
 *
 * NOT emitted: the OperationCompletion event, which the Dishwasher / Laundry
 * Washer / Laundry Dryer device types mandate (cert PICS), because the param
 * model cannot tell a natural finish from a Stop command. A product that needs
 * cert has its driver signal completion and emits the event itself. */
class OperationalStateDelegate : public chip::app::Clusters::OperationalState::Delegate {
public:
    explicit OperationalStateDelegate(app_driver_param_id_t state_param) : m_param(state_param) {}

    chip::app::DataModel::Nullable<uint32_t> GetCountdownTime() override
        { return chip::app::DataModel::NullNullable; }

    CHIP_ERROR GetOperationalStateAtIndex(
        size_t index, chip::app::Clusters::OperationalState::GenericOperationalState &out) override
    {
        using E = chip::app::Clusters::OperationalState::OperationalStateEnum;
        static const E k[] = { E::kStopped, E::kRunning, E::kPaused, E::kError };
        /* OperationalState terminates its list loop on NOT_FOUND (ModeBase uses
         * PROVIDER_LIST_EXHAUSTED — do not unify the two). */
        if (index >= sizeof(k) / sizeof(k[0])) return CHIP_ERROR_NOT_FOUND;
        out = chip::app::Clusters::OperationalState::GenericOperationalState(chip::to_underlying(k[index]));
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetOperationalPhaseAtIndex(size_t, chip::MutableCharSpan &) override
        { return CHIP_ERROR_NOT_FOUND; }

    void HandleStartStateCallback(chip::app::Clusters::OperationalState::GenericOperationalError &err) override { set(1, err); }
    void HandleStopStateCallback(chip::app::Clusters::OperationalState::GenericOperationalError &err) override { set(0, err); }
    void HandlePauseStateCallback(chip::app::Clusters::OperationalState::GenericOperationalError &err) override { set(2, err); }
    void HandleResumeStateCallback(chip::app::Clusters::OperationalState::GenericOperationalError &err) override { set(1, err); }

private:
    app_driver_param_id_t m_param;
    void set(uint8_t s, chip::app::Clusters::OperationalState::GenericOperationalError &err) {
        app_driver_param_val_t v = { .u8 = s };
        app_driver_set_param(m_param, v, APP_DRIVER_SOURCE_LOCAL);
        err.Set(chip::to_underlying(chip::app::Clusters::OperationalState::ErrorStateEnum::kNoError));
    }
};

/* param -> cluster. Runs on the Matter thread (ScheduleWork target). arg packs
 * (endpoint_id << 8 | state). Updates the OperationalState Instance esp_matter
 * built for the endpoint. */
inline void operational_state_push(intptr_t arg)
{
    uint8_t state = (uint8_t)(arg & 0xff);
    uint16_t ep   = (uint16_t)(arg >> 8);
    esp_matter::cluster_t *cl = esp_matter::cluster::get(ep, chip::app::Clusters::OperationalState::Id);
    if (!cl) return;
    auto *inst = static_cast<chip::app::Clusters::OperationalState::Instance *>(
        esp_matter::cluster::get_delegate_managed_instance(cl));
    if (!inst) return;
    CHIP_ERROR e = inst->SetOperationalState(state);
    if (e != CHIP_NO_ERROR) ESP_LOGW("zc_matter", "SetOperationalState failed:%" CHIP_ERROR_FORMAT, e.Format());
}

inline intptr_t pack_ep_u8(uint16_t ep, uint8_t v) { return (intptr_t)(((intptr_t)ep << 8) | v); }

/* A selectable mode: user-visible label, the value reported/accepted on the
 * wire, and its ModeTag (uint16 — a cluster-specific or common tag). Each
 * derived Mode cluster requires >=1 mode carry its Normal tag (0x4000), so a
 * block's first entry should use that. Blocks define a static array of these
 * (the mode list is product data). */
struct ModeOption { const char *label; uint8_t value; uint16_t tag; };

/* param -> cluster for a mode cluster. Templated on the cluster id because
 * DishwasherMode / LaundryWasherMode / … are distinct clusters; the template
 * gives a concrete void(intptr_t) for ScheduleWork per device type. */
template <uint32_t ClusterId>
inline void mode_push(intptr_t arg)
{
    uint8_t mode = (uint8_t)(arg & 0xff);
    uint16_t ep  = (uint16_t)(arg >> 8);
    esp_matter::cluster_t *cl = esp_matter::cluster::get(ep, ClusterId);
    if (!cl) return;
    auto *inst = static_cast<chip::app::Clusters::ModeBase::Instance *>(
        esp_matter::cluster::get_delegate_managed_instance(cl));
    if (inst) inst->UpdateCurrentMode(mode);
}

} // namespace zc

namespace chip { namespace app { namespace Clusters { namespace ModeBase {

/* ── ModeBase (DishwasherMode, LaundryWasherMode, OvenMode, …) ─────────────
 * Defined in the ModeBase namespace so detail::Structs / StatusCode / Commands
 * resolve exactly as upstream. Reusable across every *Mode cluster; the mode
 * list is passed in as data. ChangeToMode writes the mode param with
 * APP_DRIVER_SOURCE_LOCAL, so the matter driver_cb reflects it to the cluster
 * (zc::mode_push) — same one-way-in, driver_cb-reflects-out shape as
 * OperationalState. */
class ZcModeDelegate : public Delegate {
public:
    ZcModeDelegate(app_driver_param_id_t mode_param, const zc::ModeOption *opts, uint8_t count)
        : m_param(mode_param), m_opts(opts), m_count(count) {}

    CHIP_ERROR Init() override { return CHIP_NO_ERROR; }

    CHIP_ERROR GetModeLabelByIndex(uint8_t i, chip::MutableCharSpan &label) override {
        if (i >= m_count) return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
        return chip::CopyCharSpanToMutableCharSpan(chip::CharSpan::fromCharString(m_opts[i].label), label);
    }
    CHIP_ERROR GetModeValueByIndex(uint8_t i, uint8_t &value) override {
        if (i >= m_count) return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
        value = m_opts[i].value; return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetModeTagsByIndex(uint8_t i, chip::app::DataModel::List<detail::Structs::ModeTagStruct::Type> &tags) override {
        if (i >= m_count) return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
        /* one tag per mode, from the block's data. The list is per-index but
         * consumed synchronously, so a single reused static entry is safe. */
        static detail::Structs::ModeTagStruct::Type s_tag;
        s_tag.value = m_opts[i].tag;
        tags = chip::app::DataModel::List<detail::Structs::ModeTagStruct::Type>(&s_tag, 1);
        return CHIP_NO_ERROR;
    }
    void HandleChangeToMode(uint8_t newMode, Commands::ChangeToModeResponse::Type &response) override {
        bool ok = false;
        for (uint8_t i = 0; i < m_count; i++) if (m_opts[i].value == newMode) { ok = true; break; }
        if (!ok) { response.status = chip::to_underlying(StatusCode::kUnsupportedMode); return; }
        app_driver_param_val_t v = { .u8 = newMode };
        app_driver_set_param(m_param, v, APP_DRIVER_SOURCE_LOCAL);
        response.status = chip::to_underlying(StatusCode::kSuccess);
    }
private:
    app_driver_param_id_t m_param;
    const zc::ModeOption *m_opts;
    uint8_t m_count;
};

}}}} // namespace chip::app::Clusters::ModeBase

namespace zc { using ModeDelegate = chip::app::Clusters::ModeBase::ZcModeDelegate; }

/* ── ClosureControl (closure device type) ──────────────────────────────────
 * Command-driven: MoveTo(FullyOpen/FullyClosed)/Stop map to a signed motor
 * speed param (+open / -close / 0 stop). Unsupported target positions
 * (Pedestrian/Ventilation/Signature) and a value-less MoveTo are no-ops rather
 * than a surprise open. Defined in the cluster namespace so the SDK types
 * resolve as upstream; written with APP_DRIVER_SOURCE_LOCAL like the delegates
 * above, so the driver_cb still reflects a local change. */
namespace chip { namespace app { namespace Clusters { namespace ClosureControl {

class ZcClosureControlDelegate : public ClosureControlClusterDelegate {
public:
    explicit ZcClosureControlDelegate(app_driver_param_id_t motor_param) : m_param(motor_param) {}

    Protocols::InteractionModel::Status HandleMoveToCommand(const Optional<TargetPositionEnum> &position,
        const Optional<bool> &, const Optional<Globals::ThreeLevelAutoEnum> &) override {
        if (!position.HasValue()) return Protocols::InteractionModel::Status::Success;
        int16_t s;
        switch (position.Value()) {
            case TargetPositionEnum::kMoveToFullyClosed: s = -1000; break;
            case TargetPositionEnum::kMoveToFullyOpen:   s = 1000;  break;
            default: return Protocols::InteractionModel::Status::Success;
        }
        app_driver_param_val_t v = { .i16 = s };
        app_driver_set_param(m_param, v, APP_DRIVER_SOURCE_LOCAL);
        return Protocols::InteractionModel::Status::Success;
    }
    Protocols::InteractionModel::Status HandleStopCommand() override {
        app_driver_param_val_t v = { .i16 = 0 };
        app_driver_set_param(m_param, v, APP_DRIVER_SOURCE_LOCAL);
        return Protocols::InteractionModel::Status::Success;
    }
    Protocols::InteractionModel::Status HandleCalibrateCommand() override { return Protocols::InteractionModel::Status::Success; }
    bool IsReadyToMove() override { return true; }
    ElapsedS GetCalibrationCountdownTime() override { return ElapsedS(0); }
    ElapsedS GetMovingCountdownTime() override { return ElapsedS(0); }
    ElapsedS GetWaitingForMotionCountdownTime() override { return ElapsedS(0); }
private:
    app_driver_param_id_t m_param;
};

}}}} // namespace chip::app::Clusters::ClosureControl

namespace zc { using ClosureControlDelegate = chip::app::Clusters::ClosureControl::ZcClosureControlDelegate; }

/* ── ClosureDimension (closure_panel device type) ──────────────────────────
 * SetTarget's Percent100ths (0-10000) maps directly onto a curtain-style
 * position param (0 closed .. 10000 open). Step is a no-op for this simple
 * panel. */
namespace chip { namespace app { namespace Clusters { namespace ClosureDimension {

class ZcClosureDimensionDelegate : public DelegateBase {
public:
    explicit ZcClosureDimensionDelegate(app_driver_param_id_t position_param) : m_param(position_param) {}

    Protocols::InteractionModel::Status HandleSetTarget(const Optional<Percent100ths> &position,
        const Optional<bool> &, const Optional<Globals::ThreeLevelAutoEnum> &) override {
        if (position.HasValue()) {
            app_driver_param_val_t v = { .u16 = position.Value() };
            app_driver_set_param(m_param, v, APP_DRIVER_SOURCE_LOCAL);
        }
        return Protocols::InteractionModel::Status::Success;
    }
    Protocols::InteractionModel::Status HandleStep(const StepDirectionEnum &, const uint16_t &,
        const Optional<Globals::ThreeLevelAutoEnum> &) override {
        return Protocols::InteractionModel::Status::Success;
    }
private:
    app_driver_param_id_t m_param;
};

}}}} // namespace chip::app::Clusters::ClosureDimension

namespace zc { using ClosureDimensionDelegate = chip::app::Clusters::ClosureDimension::ZcClosureDimensionDelegate; }

/* ── Chime (chime device type) ─────────────────────────────────────────────
 * Advertises one installed sound; PlayChimeSound fires a speaker tone param
 * and schedules a timer to silence it after ~2s (a chime, not a stuck tone).
 * The timer callback carries `this` so it reads the instance's own param. */
namespace chip { namespace app { namespace Clusters { namespace Chime {

class ZcChimeDelegate : public ChimeDelegate {
public:
    explicit ZcChimeDelegate(app_driver_param_id_t tone_param) : m_param(tone_param) {}

    CHIP_ERROR GetChimeSoundByIndex(uint8_t chimeIndex, uint8_t &chimeID, MutableCharSpan &name) override {
        if (chimeIndex >= 1) return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
        chimeID = 1;
        return chip::CopyCharSpanToMutableCharSpan(chip::CharSpan::fromCharString("Ding Dong"), name);
    }
    CHIP_ERROR GetChimeIDByIndex(uint8_t chimeIndex, uint8_t &chimeID) override {
        if (chimeIndex >= 1) return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
        chimeID = 1;
        return CHIP_NO_ERROR;
    }
    Protocols::InteractionModel::Status PlayChimeSound(uint8_t) override {
        app_driver_param_val_t on = { .u32 = 1000 };
        app_driver_fire_event(m_param, on, APP_DRIVER_SOURCE_LOCAL);
        chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(2),
            [](chip::System::Layer *, void *ctx) {
                auto *self = static_cast<ZcChimeDelegate *>(ctx);
                app_driver_param_val_t off = { .u32 = 0 };
                app_driver_fire_event(self->m_param, off, APP_DRIVER_SOURCE_LOCAL);
            }, this);
        return Protocols::InteractionModel::Status::Success;
    }
private:
    app_driver_param_id_t m_param;
};

}}}} // namespace chip::app::Clusters::Chime

namespace zc { using ChimeDelegate = chip::app::Clusters::Chime::ZcChimeDelegate; }
