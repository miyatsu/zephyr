#ifndef LOG_HOOK_H
#define LOG_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int _prf(int (*func)(), void *dest,
				const char *format, va_list vargs);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LOG_HOOK_H */

