inc_base += meson.current_source_dir()

# -Dboards=yunzii-b68 configures only that board (an empty list, the default, configures all of them)
selected_boards = get_option('boards')

foreach board_dir : ['example', 'nuphy-air60', 'eyooso-z11', 'genesis-thor-300', 'ciy-x77', 'yunzii-b68']
    if selected_boards.length() == 0 or selected_boards.contains(board_dir)
        subdir(board_dir)
    endif
endforeach
