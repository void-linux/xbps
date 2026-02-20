src = [
  'main.c',
  'check.c',
  'check_pkg_files.c',
  'check_pkg_alternatives.c',
  'check_pkg_rundeps.c',
  'check_pkg_symlinks.c',
  'check_pkg_unneeded.c',
]

install_man('xbps-pkgdb.1')
executable('xbps-pkgdb', src, dependencies: libxbps_dep, install: true)
