subdir('external')
subdir('compat')

subdir('portableproplib')
subdir('fetch')

src = [
  'package_configure.c',
  'package_config_files.c',
  'package_orphans.c',
  'package_remove.c',
  'package_state.c',
  'package_msg.c',
  'package_unpack.c',
  'package_register.c',
  'package_script.c',
  'verifysig.c',
  'transaction_commit.c',
  'transaction_prepare.c',
  'transaction_ops.c',
  'transaction_store.c',
  'transaction_check_replaces.c',
  'transaction_check_revdeps.c',
  'transaction_check_conflicts.c',
  'transaction_check_shlibs.c',
  'transaction_files.c',
  'transaction_fetch.c',
  'transaction_pkg_deps.c',
  'transaction_internalize.c',
  'pubkey2fp.c',
  'package_fulldeptree.c',
  'download.c',
  'initend.c',
  'pkgdb.c',
  'plist.c',
  'plist_find.c',
  'plist_match.c',
  'archive.c',
  'plist_remove.c',
  'plist_fetch.c',
  'util.c',
  'util_path.c',
  'util_hash.c',
  'repo.c',
  'repo_sync.c',
  'rpool.c',
  'cb_util.c',
  'proplib_wrapper.c',
  'package_alternatives.c',
  'conf.c',
  'log.c',
  external_files,
  compat_files,
]

libxbps = library(
  'xbps',
  src,
  xbps_h_configure_target,
  version: '5.0.1',
  include_directories: [xbps_include, 'fetch', 'portableproplib'],
  link_with: [libprop, libfetch],
  dependencies: [libarchive, libssl, libcrypto],
  install: true,
)

pkg = import('pkgconfig')
pkg.generate(
  libxbps,
  name: 'XBPS API Library',
  description: 'The X Binary Package System library',
  filebase: 'libxbps',
)

libxbps_dep = declare_dependency(
  link_with: libxbps,
  include_directories: xbps_include,
  dependencies: [libarchive, libssl, libcrypto],
)
