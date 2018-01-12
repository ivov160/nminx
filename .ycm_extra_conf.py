import os
import ycm_core

def FlagsForFile( filename, **kwargs ):
    return {
        'flags':  
        [
        '-W',
        '-Wall',
        '-Wpointer-arith',
        '-Wno-unused-parameter',
        '-Wunused-function',
        '-Wunused-variable',
        '-Wunused-value',
        '-Wpedantic',
        '-Wextra',
        #'-Werror',
        '-DNDEBUG',
        '-x',
        'c++',
        # '--sysroot=/',
        # '-Wall',
        # '-Wextra',
        # '-Werror',
        # '-Wc++98-compat',
        # '-Wno-long-long',
        # '-Wno-variadic-macros',
        # '-fexceptions',
        # '-DNDEBUG',
        # '-DUSE_CLANG_COMPLETER',
        # '-std=c99',
        # '-x',
        # 'c'
        '-I/usr/lib/gcc/x86_64-pc-linux-gnu/7.2.0/include/g++-v7/',
        '-I./include',
        '-I/usr/include',
        '-I/usr/local/include',
        '-I./third_party/nginx/src/core',
        '-I./third_party/nginx/src/os',
        '-I./third_party/nginx/src/event',
        '-I./third_party/nginx/src/http',
        '-I./third_party/nginx/src/stream',
        '-I./third_party/nginx/src/misc',
        '-I./third_party/nginx/src/mail',
        ],
        'do_cache': False
    }

