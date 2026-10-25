tests = {
  'lexer': 'lexer_test.c',
  'parser': 'parser_test.c',
  'sema': 'sema_test.c',
  'diagnostic': 'diagnostic_test.c',
}

foreach name, source : tests
  exe = executable(
    name + '_test',
    source,
    dependencies: nox_dep,
  )
  test(name, exe)
endforeach

test(
  'golden',
  find_program('sh'),
  args: [
    files('run_golden.sh'),
    nox_exe,
    meson.current_source_dir() / 'golden',
  ],
  depends: nox_exe,
)
