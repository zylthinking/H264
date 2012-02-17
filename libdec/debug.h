#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdarg.h>
#ifdef UNICODE
 typedef wchar_t char_t;
 #define tsprintf swprintf
 #define tsnprintf_s _snwprintf_s
 #define tfprintf fwprintf
 #define tsscanf swscanf
 #define __l(x) L ## x
 #define _l(x) __l(x)
#else
 typedef char char_t;
 #define tsprintf sprintf
 #define tsnprintf_s _snprintf_s
 #define tfprintf fprintf
 #define tsscanf sscanf
 #define _l(x) x
#endif

#ifndef FFDEBUG
#define FFDEBUG 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int allowDPRINTF;

#if FFDEBUG!=0
void DPRINTF(const char_t *fmt,...);
void DPRINTFva(const char_t *fmt,va_list valist);
void DPRINTFA(const char *fmt,...);
void DPRINTFvaA(const char *fmt,va_list valist);
void DPRINTFW(const wchar_t *fmt,...);
void DPRINTFvaW(const wchar_t *fmt,va_list valist);
#else
static __inline void DPRINTF(const char_t *fmt,...) {}
static __inline void DPRINTFva(const char_t *fmt,va_list valist) {}
static __inline void DPRINTFA(const char *fmt,...) {}
static __inline void DPRINTFvaA(const char *fmt,va_list valist) {}
static __inline void DPRINTFW(const wchar_t *fmt,...) {}
static __inline void DPRINTFvaW(const wchar_t *fmt,va_list valist) {}
#endif

#ifdef __cplusplus
}
#endif

#endif
