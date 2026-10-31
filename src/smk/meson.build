inc_base += meson.current_source_dir()

src_smk += files(
    'host.c',
    'usb.c',
    'matrix.c',
    'layer.c',
    'tapping.c',
    'macro.c',
    'keymap.c',
    'keymap_store.c',
    'encoder.c',
    'report.c',
    'keyboard.c',
    'console.c',
    'debug_sink.c',
    'settings.c',
    'led_effect.c',
    'sleep.c',
    'tick.c',
    'recovery.c',
    'timer.c',
)

# Vial/VIA protocol, built only for a board that has a `vial_json` option
src_via = files(
    'via.c',
    'vial.c',
    'raw_hid.c',
    'hud.c',
)
