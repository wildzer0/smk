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
        },
        'options' : {
            'keymap_sectors' : '3',  # 8 layers x 5 x 16 x 2 B + header = 1292 B per copy, two copies
        },
    },
}
