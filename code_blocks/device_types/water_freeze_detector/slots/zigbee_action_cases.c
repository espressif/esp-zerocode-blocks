if (callback_id == EZB_ZCL_CORE_IAS_ZONE_ENROLL_RSP_CB_ID) {
    const ezb_zcl_ias_zone_enroll_rsp_message_t *msg =
        (const ezb_zcl_ias_zone_enroll_rsp_message_t *)message;
    if (msg && msg->info.dst_ep == s_{{prefix_lc}}_zb_endpoint) {
        if (msg->in.payload.enroll_rsp_code == EZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS) {
            ESP_LOGI(TAG, "{{prefix_lc}}: IAS Zone enrolled (zone id %d)",
                     msg->in.payload.zone_id);
        } else {
            ESP_LOGW(TAG, "{{prefix_lc}}: IAS Zone enroll rejected (code %d)",
                     msg->in.payload.enroll_rsp_code);
        }
        return;
    }
}
