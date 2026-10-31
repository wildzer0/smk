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
            'default' : {
                'dir' : kb_dir / 'layouts/default',
                'src' : files('layouts/default/layout.c'),
            },
            # types the matrix coordinates of every key pressed: finds the key -> (col, row) map
            'probe' : {
                'dir' : kb_dir / 'layouts/probe',
                'src' : files('layouts/probe/layout.c'),
            },
        },
        'options' : {
            'keymap_sectors' : '3',  # 8 layers x 5 x 16 x 2 B + header = 1292 B per copy, two copies
            'vial_json' : 'vial.json',
            'vial_uid' : '0x46ecd0565df84fa8',
            # (row, col) cells to hold to unlock: Esc and Enter
            'vial_unlock' : '0,0;2,13',
        },
    },
}
