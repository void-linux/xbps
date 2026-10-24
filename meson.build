project(
  'xbps',
  'c',
  default_options: ['warning_level=3', 'c_std=c99'],
  license: 'BSD-2-Clause, BSD-3-Clause, ISC',
  license_files: 'LICENSE',
  meson_version: '>=1.3.0',
  version: '0.60',
)

# == Set preprocessor defines used by XBPS ====================================

CC = meson.get_compiler('c')

if CC.has_function_attribute('visibility:hidden')
  add_project_arguments('-DHAVE_VISIBILITY', language: 'c')
endif

prefix = get_option('prefix')

dbdir = get_option('dbdir')
if dbdir == ''
  dbdir = prefix / get_option('localstatedir') / 'db/xbps'
else
  dbdir = prefix / dbdir
endif

CC.has_function_attribute('unused', required: true)

add_project_arguments(
  '-D_GNU_SOURCE',
  '-DXBPS_SYSCONF_PATH="' + prefix / get_option('sysconfdir') / 'xbps.d' + '"',
  '-DXBPS_SYSDEFCONF_PATH="' + prefix / get_option('datadir') / 'xbps.d' + '"',
  '-DXBPS_VERSION="' + meson.project_version() + '"',
  '-DXBPS_META_PATH="' + dbdir + '"',
  '-DUNUSED=__attribute__((__unused__))',
  language: 'c',
)

if get_option('enable-fulldebug')
  add_project_arguments('-DFULL_DEBUG', language: 'c')
elif get_option('debug')
  add_project_arguments('-DDEBUG', language: 'c')
endif

if build_machine.system() == 'linux'
  add_project_arguments('-D_XOPEN_SOURCE=700', language: 'c')
endif

# == Set extra flags for warnings, fortifying and linking =====================

# Enable extra warnings if supported by compiler.
if get_option('warning_level') == '3'
  foreach warn : [
    'shadow',
    'format=2',
    'missing-prototypes',
    'missing-declarations',
    'nested-externs',
    'vla',
    'overlength-strings',
    'unsafe-loop-optimizations',
    'undef',
    'sign-compare',
    'missing-include-dirs',
    'old-style-definition',
    'init-self',
    'redundant-decls',
    'float-equal',
    'missing-noreturn',
    'cast-align',
    'cast-qual',
    'pointer-arith',
    'comment',
    'declaration-after-statement',
    'write-strings',
    'stack-protector',
  ]
    if CC.has_argument('-W' + warn)
      add_project_arguments('-W' + warn, language: 'c')
    endif
  endforeach
endif

if CC.has_argument('-finline-functions')
  add_project_arguments('-finline-functions', language: 'c')
endif
if CC.has_argument('-fstack-protector-strong')
  add_project_arguments('-fstack-protector-strong', language: 'c')
elif CC.has_argument('-fstack-protector')
  add_project_arguments('-fstack-protector', language: 'c')
  if CC.get_id() == 'gcc'
    add_project_arguments('--param', 'ssp-buffer-size=1', language: 'c')
  endif
endif

if CC.has_multi_link_arguments('-z', 'relro')
  add_project_link_arguments('-z', 'relro', language: 'c')
endif
if CC.has_multi_link_arguments('-z', 'now')
  add_project_link_arguments('-z', 'now', language: 'c')
endif

# openssl 3 compatibility
add_project_arguments('-Wno-error=deprecated-declarations', language: 'c')

# TODO: vcs_tag() & XBPS_GIT
# TODO: export_dynamic
# TODO: does LTO require gcc-ar and gcc-ranlib?

# == Get dependencies =========================================================

# Import all dependencies. If provided through a subproject, skip building tests
# and additional executables.
libssl = dependency('libssl', version: '>=3.0.0', default_options: 'c_std=none')
libcrypto = dependency('libcrypto', default_options: 'c_std=none')
zlib = dependency('zlib', default_options: ['c_std=none', 'tests=disabled'])
libarchive = dependency(
  'libarchive',
  version: '>=3.3.3',
  default_options: ['c_std=none', 'progs=[]'],
)

# == Check for nonportable functions ==========================================

vasprintf_code = '''
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
int main(void) {
	va_list ap;
	vasprintf(NULL, NULL, ap);
	return 0;
}
'''

strcasestr_code = '''
#define _GNU_SOURCE
#include <string.h>
int main(void) {
	const char *h = "NEEDCOFEE";
	const char *n = "IneedCoffee";
	strcasestr(n, h);
	return 0;
}
'''

strlcpy_code = '''
#define _GNU_SOURCE
#include <string.h>
int main(void) {
	const char s1[] = "foo";
	char s2[10];
	strlcpy(s2, s1, sizeof(s2));
	return 0;
}
'''

strlcat_code = '''
#define _GNU_SOURCE
#include <string.h>
int main(void) {
	const char src[] = "foo";
	char dst[10];
	strlcat(dst, src, sizeof(dst));
	return 0;
}
'''

humanize_number_code = '''
#include <stdio.h>
#include <util.h>

int main(void) {
	humanize_number(NULL, 0, 0, NULL, 0, 0);
	return 0;
}
'''

fdatasync_code = '''
#define _GNU_SOURCE
#include <unistd.h>
int main(void) {
	fdatasync(0);
	return 0;
}
'''

# Link against util if available
util_lib = CC.find_library('util', required: false)
util_lib_dependency = util_lib.found() ? {'dependencies': util_lib} : {}

# This variable is reused in lib/compat/ to use compatibility replacements
# for unavailable functions.
compatibility_functions = {
  'vasprintf': CC.compiles(vasprintf_code, name: 'vasprintf() available'),
  'strcasestr': CC.compiles(strcasestr_code, name: 'strcasestr() available'),
  'strlcpy': CC.compiles(strlcpy_code, name: 'strlcpy() available'),
  'strlcat': CC.compiles(strlcat_code, name: 'strlcat() available'),
  'humanize_number': CC.compiles(
    humanize_number_code,
    name: 'humanize_number() available',
    kwargs: util_lib_dependency,
  ),
}

if compatibility_functions['vasprintf']
  add_project_arguments('-DHAVE_VASPRINTF', language: 'c')
endif
if compatibility_functions['strcasestr']
  add_project_arguments('-DHAVE_STRCASESTR', language: 'c')
endif
if compatibility_functions['strlcpy']
  add_project_arguments('-DHAVE_STRLCPY', language: 'c')
endif
if compatibility_functions['strlcat']
  add_project_arguments('-DHAVE_STRLCAT', language: 'c')
endif
if compatibility_functions['humanize_number']
  add_project_arguments('-DHAVE_HUMANIZE_NUMBER', language: 'c')
endif

if CC.compiles(fdatasync_code, name: 'fdatasync() available')
  add_project_arguments('-DHAVE_FDATASYNC', language: 'c')
endif

# == Build subdirectories =====================================================

subdir('include')
subdir('lib')
subdir('bin')
subdir('data')

if get_option('enable-api-docs')
  subdir('doc')
endif
if get_option('enable-tests')
  error(
    'Tests are not implemented in Meson! They heavily rely on in-source',
    'builds, which Meson does not support.',
  )
  # subdir('tests')
endif
