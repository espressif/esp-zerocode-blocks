/* OTA Requestor is auto-initialized by esp_matter::start() when
 * CONFIG_ENABLE_OTA_REQUESTOR=y is set in sdkconfig (this block's
 * sdkconfig.defaults sets it — CHIP's own default is n). No additional
 * code is needed here. To surface OTA progress, hook
 * chip::OTAImageProcessorImpl events post-generation. */
