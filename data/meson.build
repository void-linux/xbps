install_man('xbps.d.5')
install_data(
  '60:ae:0c:d6:f0:95:17:80:bc:93:46:7a:89:af:a3:2d.plist',
  install_dir: dbdir,
)
install_data(
  'repod-main.conf',
  'xbps.conf',
  install_dir: get_option('datadir') / 'xbps.d',
  rename: ['00-repository-main.conf', 'xbps.conf'],
)
install_data(
  '_xbps',
  '_xbps_src',
  install_dir: get_option('datadir') / 'zsh/site-functions',
)
install_data(
  'xbps.bash',
  install_dir: get_option('datadir') / 'bash-completion/completions',
  rename: 'xbps',
)

foreach file : [
  'xbps-checkvers',
  'xbps-create',
  'xbps-dgraph',
  'xbps-install',
  'xbps-pkgdb',
  'xbps-query',
  'xbps-reconfigure',
  'xbps-remove',
  'xbps-rindex',
]
  install_symlink(
    file,
    pointing_to: 'xbps',
    install_dir: get_option('datadir') / 'bash-completion/completions',
  )
endforeach
