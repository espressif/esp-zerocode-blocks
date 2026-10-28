static uint16_t s_{{prefix_lc}}_endpoint_id = 0;
/* 1 when the instance reports a measured position separately (current_param). */
#define {{prefix}}_WC_SPLIT (0{{#if cfg.current_param}} + 1{{/if}})
{{#if cfg.stop_param}}
/* StopMotion reaches the product as an event; CHIP then sets Target := Current. */
class {{prefix_lc}}_wc_delegate_t : public chip::app::Clusters::WindowCovering::Delegate {
public:
    CHIP_ERROR HandleMovement(chip::app::Clusters::WindowCovering::WindowCoveringType) override { return CHIP_NO_ERROR; }
    CHIP_ERROR HandleStopMotion() override
    {
        app_driver_param_val_t v = { .u8 = 1 };
        app_driver_fire_event({{cfg.stop_param}}, v, s_handle);
        return CHIP_NO_ERROR;
    }
};
static {{prefix_lc}}_wc_delegate_t s_{{prefix_lc}}_wc_delegate;
{{/if}}
