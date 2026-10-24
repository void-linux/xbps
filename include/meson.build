xbps_h_configure_target = configure_file(
  input: 'xbps.h.in',
  output: 'xbps.h',
  copy: true,
  install: true,
  install_dir: get_option('includedir'),
)

install_subdir('xbps', install_dir: get_option('includedir'))

xbps_include = include_directories('.')
