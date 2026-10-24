src = ['common.c', 'fetch.c', 'file.c', 'ftp.c', 'http.c']

strnstr_code = '''
#include <string.h>
int main(void) {
	const char big[] = "foo";
	const char little[] = "f";
	strnstr(big, little, 3);
	return 0;
}
'''

enable_strnstr = []
CC = meson.get_compiler('c')
if CC.links(strnstr_code, name: 'strnstr() available')
  enable_strnstr = '-DHAVE_STRNSTR'
endif

ftperr = custom_target(
  'ftperr',
  command: [files('errlist.sh'), 'ftp_errlist', 'FTP', '@INPUT@'],
  capture: true,
  input: 'ftp.errors',
  output: 'ftperr.h',
)

httperr = custom_target(
  'httperr',
  command: [files('errlist.sh'), 'http_errlist', 'HTTP', '@INPUT@'],
  capture: true,
  input: 'http.errors',
  output: 'httperr.h',
)

libfetch = static_library(
  'fetch',
  src,
  ftperr,
  httperr,
  c_args: ['-DFTP_COMBINE_CWDS', '-DINET6', '-DWITH_SSL', enable_strnstr],
  gnu_symbol_visibility: 'hidden',
  include_directories: [xbps_include, '.'],
)
