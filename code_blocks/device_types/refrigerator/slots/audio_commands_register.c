/* {{prefix_lc}}: voice vocabulary — every phrase is a cfg param so a product
 * can rename what it answers to. MultiNet 7 derives phonemes from plain
 * English text via its bundled g2p. */
esp_mn_commands_add({{cfg.audio_cmd_base}} + 0, "{{cfg.say_set_the_fridge_to_normal}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 1, "{{cfg.say_set_the_fridge_to_energy_save}}");
esp_mn_commands_add({{cfg.audio_cmd_base}} + 2, "{{cfg.say_set_the_fridge_to_rapid_cool}}");
