src = [
  'main.c',
  'clean-cache.c',
  '../xbps-install/question.c',
  '../xbps-install/util.c',
  '../xbps-install/transaction.c',
]

install_man('xbps-remove.1')
executable('xbps-remove', src, dependencies: libxbps_dep, install: true)
