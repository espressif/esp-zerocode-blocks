if (endpoint_id == s_{{prefix_lc}}_endpoint_id) {
    if (cluster_id == chip::app::Clusters::OnOff::Id &&
        attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
        app_driver_param_val_t pv = { .b = val->val.b };
        app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    } else if (cluster_id == chip::app::Clusters::LevelControl::Id &&
               attribute_id == chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id) {
        app_driver_param_val_t pv = { .u8 = val->val.u8 };
        app_driver_set_param({{cfg.brightness_param}}, pv, s_handle);
    } else if (cluster_id == chip::app::Clusters::ColorControl::Id) {
        using namespace chip::app::Clusters::ColorControl::Attributes;
        /* The driver speaks hue/sat; the endpoint accepts HS, XY and colour
           temperature. The server writes one attribute at a time and skips
           unchanged ones, so the values are not consistent until it has
           finished — sync once afterwards from the stored attributes. */
        if (attribute_id == ColorMode::Id || attribute_id == CurrentHue::Id ||
            attribute_id == CurrentSaturation::Id || attribute_id == CurrentX::Id ||
            attribute_id == CurrentY::Id || attribute_id == ColorTemperatureMireds::Id) {
            {{prefix_lc}}_schedule_color_sync();
        }
    }
}
