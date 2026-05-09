doxygen = find_program('doxygen', required: true)
graphviz_dot = find_program('dot', required: true)

dot_format = get_option('api-docs-format')

foreach dotfile : [
  'xbps_transaction_dictionary',
  'xbps_pkgdb_dictionary',
  'xbps_pkg_props_dictionary',
  'xbps_pkg_files_dictionary',
  'xbps_binary_pkg_content',
]
  custom_target(
    command: [graphviz_dot, '-T' + dot_format, '@INPUT@', '-o', '@OUTPUT@'],
    input: dotfile + '.dot',
    output: dotfile + '.' + dot_format,
    build_by_default: true,
  )
endforeach

sed = find_program('sed', required: true)
distver = get_option('api-docs-distver')
if distver == ''
  command = [
    'sh',
    '-c',
    sed.full_path() + ' -e \'s|@@PROJECT_NUMBER@@|$(date +%Y%m%d)|\' @INPUT@',
  ]
else
  command = [sed, '-e', 's|@@PROJECT_NUMBER@@|' + distver + '|', '@INPUT@']
endif
xbps_api_doxyfile = custom_target(
  command: command,
  input: 'xbps_api_doxyfile.in',
  capture: true,
  output: 'xbps_api_doxyfile',
)

run_target('doxygen', command: [doxygen, xbps_api_doxyfile])
