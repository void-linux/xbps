src = ['main.c']

install_man('xbps-checkvers.1')
executable(
  'xbps-checkvers',
  src,
  c_args: '-Wno-deprecated-declarations',
  dependencies: libxbps_dep,
  install: true,
)
