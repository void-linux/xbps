src = [
  'main.c',
  'transaction.c',
  'question.c',
  'fetch_cb.c',
  'state_cb.c',
  'util.c',
]

clock_gettime_code = '''
#include <time.h>
int main(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return 0;
}
'''

CC = meson.get_compiler('c')

rt = CC.find_library('rt', required: false)
rt_dep = rt.found() ? rt : []
extra_flags = []

if CC.compiles(
  clock_gettime_code,
  name: 'clock_gettime() available',
  args: '-D_GNU_SOURCE',
  dependencies: rt_dep,
)
  extra_flags = '-DHAVE_CLOCK_GETTIME'
endif

install_man('xbps-install.1')
executable(
  'xbps-install',
  src,
  c_args: extra_flags,
  dependencies: [libxbps_dep, rt_dep],
  install: true,
)
