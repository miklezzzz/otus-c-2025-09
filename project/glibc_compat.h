#ifndef GLIBC_COMPAT_H
#define GLIBC_COMPAT_H
__asm__(".symver strtol,strtol@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtol,strtol@GLIBC_2.2.5");
__asm__(".symver fprintf,fprintf@GLIBC_2.2.5");
__asm__(".symver __fprintf_chk,fprintf@GLIBC_2.2.5");
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
__asm__(".symver __memcpy_chk,memcpy@GLIBC_2.2.5");
__asm__(".symver memset,memset@GLIBC_2.2.5");
__asm__(".symver __memset_chk,memset@GLIBC_2.2.5");
#endif
