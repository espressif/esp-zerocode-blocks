if (callback_id == EZB_ZCL_CORE_OTA_UPGRADE_CLIENT_PROGRESS_CB_ID) {
    {{prefix_lc}}_zb_ota_upgrade_handler(
        (ezb_zcl_ota_upgrade_client_progress_message_t *)message);
    return;
}
if (callback_id == EZB_ZCL_CORE_OTA_UPGRADE_QUERY_NEXT_IMAGE_RSP_CB_ID) {
    {{prefix_lc}}_zb_ota_query_response_handler(
        (ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *)message);
    return;
}
