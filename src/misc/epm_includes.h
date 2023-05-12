#ifndef EPM_INCLUDES_H
#define EPM_INCLUDES_H

/* This header is to be included by all header and/or source files in the EPM
   project. */

#if __linux__
#  define EPM_PLATFORM_STR "linux"
#  define EPM_LINUX 1
#elif _WIN32
#  define EPM_PLATFORM_STR "windows"
#  define EPM_WINDOWS 1
#else
# error Only Linux and Windows are tested compilation targets.
#endif

typedef int epm_Result;
#define EPM_SUCCESS 0
#define EPM_ERROR -1
#define EPM_FAILURE -2
#define EPM_NULL_ARG -3

#include "zigil/zigil_mem.h"

#if ZGL_MEMCHECK_LEVEL != 0
// Force an error if try to use malloc(), calloc(), realloc(), or free().
// This project exclusively uses the wrappers zgl_Malloc(), zgl_Calloc(), zgl_Realloc(), and zgl_Free()
# undef malloc
# undef calloc
# undef realloc
# undef free
# define malloc(SIZE) do_not_use_malloc_directly(_)
# define calloc(NMEMB, SIZE) do_not_use_calloc_directly(_)
# define realloc(PTR, SIZE) do_not_use_realloc_directly(_)
# define free(PTR) do_not_use_free_directly(_)
#endif

#include "src/misc/log.h"
#include "src/misc/mathematics.h"
#include "fixpt.h"
#include "dibassert.h"
#include "dibstr.h"

// A big character buffer for temporary storage.
#define BIGBUF_LEN ((1<<16)-1)
extern char bigbuf[BIGBUF_LEN+1];

#endif /* EPM_INCLUDES_H */
