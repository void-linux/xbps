src = [
  'prop_array.c',
  'prop_bool.c',
  'prop_dictionary.c',
  'prop_ingest.c',
  'prop_object.c',
  'prop_stack.c',
  'prop_string.c',
  'prop_array_util.c',
  'prop_number.c',
  'prop_dictionary_util.c',
  'prop_zlib.c',
  'prop_data.c',
]

CC = meson.get_compiler('c')

rb_tree_init_code = '''
#include <sys/rbtree.h>
int main(void) {
	rb_tree_init(NULL, NULL);
	return 0;
}
'''

gcc_atomic_code = '''
int main() {
	volatile unsigned long val = 1;
	__sync_fetch_and_add(&val, 1);
	__sync_fetch_and_sub(&val, 1);
	__sync_add_and_fetch(&val, 1);
	__sync_sub_and_fetch(&val, 1);
	return 0;
}
'''

if not CC.compiles(rb_tree_init_code, name: 'rb_tree_init() available')
  src += 'rb.c'
endif

enable_atomic = []
if CC.compiles(gcc_atomic_code, name: 'GCC atomic builtins available')
  enable_atomic = '-DHAVE_ATOMICS'
endif

thread_dep = dependency('threads')

libprop = static_library(
  'portableproplib',
  src,
  c_args: ['-Wno-unused-parameter', enable_atomic],
  include_directories: [xbps_include, 'prop'],
  gnu_symbol_visibility: 'hidden',
  dependencies: [zlib, thread_dep],
)
