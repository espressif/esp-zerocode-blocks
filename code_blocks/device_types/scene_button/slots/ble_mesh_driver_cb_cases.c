if (param_id == {{cfg.position_param}}) {
    s_{{prefix_lc}}_mesh_on = !s_{{prefix_lc}}_mesh_on;
    zc_ble_mesh_publish_onoff(ZC_MESH_GROUP_ADDR, s_{{prefix_lc}}_mesh_on);
}
