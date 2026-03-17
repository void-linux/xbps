src = [
  'main.c',
  'list.c',
  'show-deps.c',
  'show-info-files.c',
  'ownedby.c',
  'search.c',
  '../xbps-install/util.c',
]

install_man('xbps-query.1')
executable('xbps-query', src, dependencies: libxbps_dep, install: true)
