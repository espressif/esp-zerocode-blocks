static uint16_t s_{{prefix_lc}}_endpoint_id = 0;
/* True while a controller's colour is being pushed to the driver, so the
   driver echo does not overwrite the server's own attributes and ColorMode. */
static bool s_{{prefix_lc}}_color_write = false;
static bool s_{{prefix_lc}}_sync_pending = false;

static bool {{prefix_lc}}_read_color_attr(uint32_t attribute_id, esp_matter_attr_val_t *out)
{
    *out = esp_matter_invalid(NULL);
    esp_err_t err = esp_matter::attribute::get_val(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::ColorControl::Id, attribute_id, out);
    if (err != ESP_OK) ESP_LOGE(TAG, "{{prefix_lc}}: ColorControl attr 0x%" PRIx32 " read failed", attribute_id);
    return err == ESP_OK;
}

/* Attribute write that must not re-enter the PRE_UPDATE slot. */
static void {{prefix_lc}}_write_color_attr(uint32_t attribute_id, esp_matter_attr_val_t val)
{
    s_from_driver = true;
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::ColorControl::Id, attribute_id, &val);
    s_from_driver = false;
}

static void {{prefix_lc}}_drive_hs(zc::Hs hs)
{
    app_driver_param_val_t ps = { .u8 = hs.sat };
    /* Hue is undefined at zero saturation; keep the driver's. */
    app_driver_param_val_t ph = {};
    if (hs.sat > 0 || app_driver_get_param({{cfg.hue_param}}, &ph) != ESP_OK) ph.u8 = hs.hue;
    s_{{prefix_lc}}_color_write = true;
    app_driver_set_param({{cfg.hue_param}}, ph, s_handle);
    app_driver_set_param({{cfg.saturation_param}}, ps, s_handle);
    s_{{prefix_lc}}_color_write = false;
}

/* Runs on the CHIP thread once the server has finished a colour write. The
   server sets ColorMode first and then the model's values, so the stored mode
   says which pair is authoritative: drive the LED from it and derive the other
   model's attributes. Done here, not in PRE_UPDATE, because the bus never
   echoes a write back to its source: the Matter solution's own set_param
   does not reach matter_driver_cb, so nothing else would fill them in. */
static void {{prefix_lc}}_sync_color(intptr_t)
{
    using namespace chip::app::Clusters::ColorControl;
    s_{{prefix_lc}}_sync_pending = false;
    esp_matter_attr_val_t mode, a, b;
    if (!{{prefix_lc}}_read_color_attr(Attributes::ColorMode::Id, &mode)) return;
    switch ((ColorModeEnum)mode.val.u8) {
    case ColorModeEnum::kCurrentHueAndCurrentSaturation: {
        if (!{{prefix_lc}}_read_color_attr(Attributes::CurrentHue::Id, &a) ||
            !{{prefix_lc}}_read_color_attr(Attributes::CurrentSaturation::Id, &b)) return;
        zc::Hs hs = { a.val.u8, b.val.u8 };
        {{prefix_lc}}_drive_hs(hs);
        uint16_t x, y;
        zc::hs_to_xy(hs.hue, hs.sat, &x, &y);
        {{prefix_lc}}_write_color_attr(Attributes::CurrentX::Id, esp_matter_uint16(x));
        {{prefix_lc}}_write_color_attr(Attributes::CurrentY::Id, esp_matter_uint16(y));
        break;
    }
    case ColorModeEnum::kCurrentXAndCurrentY: {
        if (!{{prefix_lc}}_read_color_attr(Attributes::CurrentX::Id, &a) ||
            !{{prefix_lc}}_read_color_attr(Attributes::CurrentY::Id, &b)) return;
        zc::Hs hs = zc::xy_to_hs(a.val.u16, b.val.u16);
        {{prefix_lc}}_drive_hs(hs);
        {{prefix_lc}}_write_color_attr(Attributes::CurrentHue::Id, esp_matter_uint8(hs.hue));
        {{prefix_lc}}_write_color_attr(Attributes::CurrentSaturation::Id, esp_matter_uint8(hs.sat));
        break;
    }
    case ColorModeEnum::kColorTemperatureMireds: {
        if (!{{prefix_lc}}_read_color_attr(Attributes::ColorTemperatureMireds::Id, &a)) return;
        zc::Hs hs = zc::ct_to_hs(a.val.u16);
        {{prefix_lc}}_drive_hs(hs);
        {{prefix_lc}}_write_color_attr(Attributes::CurrentHue::Id, esp_matter_uint8(hs.hue));
        {{prefix_lc}}_write_color_attr(Attributes::CurrentSaturation::Id, esp_matter_uint8(hs.sat));
        break;
    }
    default:
        ESP_LOGW(TAG, "{{prefix_lc}}: unknown ColorMode %u", mode.val.u8);
    }
}

/* Called from the ColorControl PRE_UPDATE; coalesces one command's writes
   (ColorMode, then one or two values) into a single sync after the last. */
static void {{prefix_lc}}_schedule_color_sync(void)
{
    if (s_{{prefix_lc}}_sync_pending) return;
    s_{{prefix_lc}}_sync_pending = true;
    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork({{prefix_lc}}_sync_color, 0);
    if (err != CHIP_NO_ERROR) {
        s_{{prefix_lc}}_sync_pending = false;
        ESP_LOGE(TAG, "{{prefix_lc}}: colour sync not scheduled: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

/* A hue/sat change from outside Matter (button, console, RainMaker) is
   reported in both models and ColorMode is switched to HS so a controller
   reads the right pair. Caller holds s_from_driver. */
static void {{prefix_lc}}_report_color(uint8_t hue, uint8_t sat)
{
    using namespace chip::app::Clusters::ColorControl;
    if (s_{{prefix_lc}}_color_write) return;
    uint16_t x, y;
    zc::hs_to_xy(hue, sat, &x, &y);
    esp_matter_attr_val_t vx = esp_matter_uint16(x), vy = esp_matter_uint16(y);
    esp_matter_attr_val_t mode = esp_matter_enum8((uint8_t)ColorModeEnum::kCurrentHueAndCurrentSaturation);
    esp_matter_attr_val_t emode = esp_matter_enum8((uint8_t)EnhancedColorModeEnum::kCurrentHueAndCurrentSaturation);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id, Id, Attributes::CurrentX::Id, &vx);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id, Id, Attributes::CurrentY::Id, &vy);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id, Id, Attributes::ColorMode::Id, &mode);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id, Id, Attributes::EnhancedColorMode::Id, &emode);
}
