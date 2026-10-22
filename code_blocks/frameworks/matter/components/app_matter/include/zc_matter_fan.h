/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* FanControl binding shared by every device_type block that carries the
 * cluster (fan, room_air_conditioner).
 *
 * esp_matter 1.6 serves FanControl from CHIP's code-driven FanControlCluster:
 * the cluster object holds FanMode / PercentSetting / PercentCurrent and
 * answers every read itself, so esp_matter::attribute::update() writes a
 * shadow nobody reads. Two consequences shape this class:
 *
 *  - A controller writing FanMode (Low/Medium/High) is turned into a
 *    PercentSetting INSIDE the cluster, which raises no esp_matter attribute
 *    callback. The cluster's Delegate::OnFanDriveStateChanged is the one
 *    place that sees every drive change — FanMode, PercentSetting and
 *    SpeedSetting writes alike — so the driver is applied there.
 *  - PercentCurrent (what the fan actually runs at) is the application's to
 *    report; the cluster only zeroes it when FanMode goes Off. It is set here
 *    whenever the drive is applied.
 *
 * Driver -> cluster runs on the Matter thread (Reflect() schedules it): the
 * cluster's setters assert the CHIP stack lock. Header-only and included from
 * a block's matter_includes slot, so nothing here may reference s_handle — the
 * block passes it to Attach().
 */
#pragma once

#include <esp_matter.h>
#include <esp_log.h>
#include <app_driver.h>
#include <app/clusters/fan-control-server/FanControlCluster.h>
#include <app/clusters/fan-control-server/fan-control-delegate.h>
/* Declares FanControl::SetDefaultDelegate, which esp_matter's FanControl
 * integration defines. */
#include <app/clusters/fan-control-server/fan-control-server.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <platform/CHIPDeviceLayer.h>

namespace zc {

/* The cluster object esp_matter registered for the endpoint, or NULL before the
 * Matter server has constructed it. Call with the CHIP stack lock held. */
inline chip::app::Clusters::FanControlCluster *fan_cluster(uint16_t ep)
{
    chip::app::ServerClusterInterface *c = esp_matter::data_model::provider::get_instance().registry().Get(
        chip::app::ConcreteClusterPath(ep, chip::app::Clusters::FanControl::Id));
    /* Sound cast: esp_matter's integration registers exactly this type. */
    return static_cast<chip::app::Clusters::FanControlCluster *>(c);
}

class FanDrive : public chip::app::Clusters::FanControl::Delegate {
public:
    explicit FanDrive(app_driver_param_id_t speed_param) : m_param(speed_param) {}

    /* Bind to the endpoint's cluster and seed it from the driver. Call on
     * kServerReady (Matter thread): esp_matter constructs the cluster during
     * server start-up and SetDefaultDelegate is a no-op before that. The
     * driver is the truth at boot, so the cluster follows it rather than the
     * FanMode it restored. */
    void Attach(uint16_t ep, app_driver_handle_t matter_handle)
    {
        mEndpoint = ep;
        m_handle  = matter_handle;
        if (!fan_cluster(ep)) {
            ESP_LOGW("zc_matter", "FanControl cluster not registered (ep %u)", ep);
            return;
        }
        chip::app::Clusters::FanControl::SetDefaultDelegate(ep, this);
        Push();
    }

    /* Driver -> cluster, from any thread (the matter driver callback). */
    void Reflect()
    {
        CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(push_work, reinterpret_cast<intptr_t>(this));
        if (err != CHIP_NO_ERROR) {
            ESP_LOGW("zc_matter", "fan state report not scheduled, err:%" CHIP_ERROR_FORMAT, err.Format());
        }
    }

    /* Controller -> driver. Called by the cluster on the Matter thread with the
     * stack lock held, after any FanMode / PercentSetting / SpeedSetting write
     * has been applied (PercentSetting already reflects a FanMode write). The
     * write's source is the matter solution's own handle, so the bus does not
     * echo it back to this solution. */
    void OnFanDriveStateChanged(const chip::app::Clusters::FanControl::FanDriveState &state) override
    {
        if (m_handle == 0 || state.percentSetting.IsNull()) {
            return; /* null = Auto: no auto mode in the driver, keep the current drive */
        }
        uint8_t pct = state.percentSetting.Value() > 100 ? 100 : state.percentSetting.Value();
        app_driver_param_val_t v = {};
        v.u8 = pct;
        app_driver_set_param(m_param, v, m_handle);
        chip::app::Clusters::FanControlCluster *c = fan_cluster(mEndpoint);
        if (c) {
            c->SetPercentCurrent(pct);
        }
    }

    /* The Step feature is not enabled on these endpoints, so the cluster never
     * accepts the command; answer it explicitly anyway. */
    chip::Protocols::InteractionModel::Status HandleStep(chip::app::Clusters::FanControl::StepDirectionEnum, bool,
                                                         bool) override
    {
        return chip::Protocols::InteractionModel::Status::UnsupportedCommand;
    }

private:
    app_driver_param_id_t m_param;
    app_driver_handle_t m_handle = 0;

    /* Matter thread. Setting PercentSetting re-derives FanMode in the cluster
     * and calls OnFanDriveStateChanged, whose driver write is then a no-op
     * (set_param dedupes the unchanged value). */
    void Push()
    {
        chip::app::Clusters::FanControlCluster *c = fan_cluster(mEndpoint);
        if (!c) {
            return;
        }
        app_driver_param_val_t v = {};
        app_driver_get_param(m_param, &v);
        uint8_t pct = v.u8 > 100 ? 100 : v.u8;
        c->SetPercentSetting(chip::app::DataModel::MakeNullable<chip::Percent>(pct));
        c->SetPercentCurrent(pct);
    }

    static void push_work(intptr_t arg)
    {
        FanDrive *self = reinterpret_cast<FanDrive *>(arg);
        if (self->m_handle != 0) {
            self->Push();
        }
    }
};

} // namespace zc
