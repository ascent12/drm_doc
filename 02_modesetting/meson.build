project('02_intro', 'c',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=c11',
    'warning_level=2',
  ],
)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

executable('02_intro', 'src/main.c', 'src/util.c',
  dependencies: dependency('libdrm')
)
