/* Read-only on purpose. The standalone SoftAP page is unauthenticated, so it
 * reports lock state but never offers an unlock control — same stance as the
 * voice binding, where the wake word is not authentication. */
{ "{{cfg.label}}", "binary", {{cfg.locked_param}}, "{{cfg.locked_param}}", 0, 0, 1, 1 },
