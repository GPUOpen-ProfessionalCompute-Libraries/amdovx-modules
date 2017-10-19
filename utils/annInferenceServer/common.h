#ifndef COMMON_H
#define COMMON_H

// Maximum number of GPUs supported
#define MAX_NUM_GPU    8

// Module library name
#ifdef __APPLE__
#define MODULE_LIBNAME "annmod.dylib"
#else
#define MODULE_LIBNAME "annmod.so"
#endif

// Useful macros
#define ERRCHK(call) if(call) return -1

// Useful functions
void info(const char * format, ...);
void warning(const char * format, ...);
void fatal(const char * format, ...);
int error(const char * format, ...);

#endif
