/* The FanControl cluster exists only once the server has started. */
if (event->Type == chip::DeviceLayer::DeviceEventType::kServerReady && s_{{prefix_lc}}_endpoint_id) {
    s_{{prefix_lc}}_fan.Attach(s_{{prefix_lc}}_endpoint_id, s_handle);
}
