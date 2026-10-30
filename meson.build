project(
  'nox',
  'c',
  version: '0.1.0',
  license: 'Apache-2.0',
  meson_version: '>=1.1',
  default_options: [
    'c_std=c23,c2x',
    'warning_level=3',
    'buildtype=debugoptimized',
    'b_ndebug=if-release',
    'default_library=static',
  ],
)

cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments(
    '-Wshadow',
    '-Wconversion',
    '-Wmissing-prototypes',
    '-Wstrict-prototypes',
    '-Wvla',
  ),
  language: 'c',
)

nox_lib = static_library(
  'nox_core',
  files(
    'src/lexer.c',
    'src/ast.c',
    'src/parser.c',
    'src/sema.c',
    'src/codegen.c',
    'src/diagnostic.c',
  ),
  include_directories: include_directories('src'),
)

nox_dep = declare_dependency(
  link_with: nox_lib,
  include_directories: include_directories('src'),
)

nox_exe = executable(
  'nox',
  'src/main.c',
  dependencies: nox_dep,
  install: true,
)

subdir('tests')
