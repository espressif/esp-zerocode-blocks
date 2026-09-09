/* {{prefix_lc}}: voice vocabulary — every phrase is a cfg param so a product
 * can rename what it answers to. MultiNet 7 derives phonemes from plain
 * English text via its bundled g2p. */
esp_mn_commands_add({{cfg.audio_cmd_base}} + 0, "{{cfg.say_turn_on_the_fan}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 1, "{{cfg.say_turn_off_the_fan}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 2, "{{cfg.say_fan_faster}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 3, "{{cfg.say_fan_slower}}");
