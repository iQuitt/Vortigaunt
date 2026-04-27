#ifndef __MEMORY_H__
#define __MEMORY_H__

    #ifndef __NEW_H__
    #ifndef __GNUC__
    #include <new.h>
    #define __NEW_H__
    #endif
    #endif

    #ifndef __MALLOC_H__
    #ifdef __APPLE__
        #include <malloc/malloc.h>
    #else
        #include <malloc.h>
    #endif
    #define __MALLOC_H__
    #endif

    #ifndef __LITHEXCEPTION_H__
    #include "lithexception.h"
    #endif


#endif //__MEMORY_H__

