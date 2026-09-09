/* {{prefix_lc}}: voice vocabulary — every phrase is a cfg param so a product
 * can rename what it answers to. MultiNet 7 derives phonemes from plain
 * English text via its bundled g2p. */
esp_mn_commands_add({{cfg.audio_cmd_base}} + 0, "{{cfg.say_turn_on_the_light}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 1, "{{cfg.say_turn_off_the_light}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 2, "{{cfg.say_make_it_brighter}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 3, "{{cfg.say_make_it_dimmer}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 4, "{{cfg.say_make_it_red}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 5, "{{cfg.say_make_it_green}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 6, "{{cfg.say_make_it_blue}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 7, "{{cfg.say_make_it_white}}");
