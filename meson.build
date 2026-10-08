project(
  'nox',
  'c',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ],
)

add_project_arguments(
  language: 'c',
)

src = files(
  'src/main.c',
  'src/lexer.c',
  'src/parser.c',
)

inc = include_directories('src')

executable(
  'nox',
  src,
  include_directories: inc,
)
