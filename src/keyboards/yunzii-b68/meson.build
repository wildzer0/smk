kb_dir = meson.current_source_dir()

keyboards += {
    'yunzii-b68' : {
        'dir' : kb_dir,
        'platform' : 'sh68f90',
        'icp_part' : 'sh68f90a',  # the part name sinodude programs it as, over ICP
        'src' : files(
            'user_matrix.c',
            'user_init.c',
            'user_sleep.c',
            'kb.c',
        ),
        'layouts' : {
            # the only release build: keyboard, media and ISP on interface 0, raw HID (VIA, Vial, HUD) on
            # interface 1. bcdDevice changes with the descriptors: 0x0200 = this layout of interfaces
            'default' : {
                'dir' : kb_dir / 'layouts/default',
                'src' : files('layouts/default/layout.c', 'layouts/default/indicators.c'),
                'args' : ['-DRAW_HID_ENABLE=1', '-DUSB_BCD_DEVICE=0x0200'],
            },
        },
        'options' : {
            # Vial's desktop app only lists a keyboard whose USB serial number contains this string
            # (vial.rocks probes the keyboard instead)
            'serial' : 'vial:f64c2b3c',
            'keymap_sectors' : '4',  # keymap 1280 B + encoder 32 B + macros 512 B + tap dances 80 B + header = 1918 B per copy, two copies
            # RGB per key: 5 rows x 16 columns; the centre of the radial effect is the middle of the 16-column board
            'led_rows' : '5',
            'led_cols' : '16',
            'led_cx2' : '15',
            'led_cy2' : '4',
            'vial_json' : 'vial.json',
            'vial_uid' : '0x46ecd0565df84fa8',
            # (row, col) cells to hold to unlock: Esc and Enter
            'vial_unlock' : '0,0;2,13',
        },
    },
}
