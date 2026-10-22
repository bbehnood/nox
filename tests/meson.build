tests = {
  'lexer': 'lexer_test.c',
  'parser': 'parser_test.c',
}

foreach name, source : tests
  exe = executable(
    name + '_test',
    source,
    dependencies: nox_dep,
  )
  test(name, exe)
endforeach
