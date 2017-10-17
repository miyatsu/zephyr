#ifndef LIBC_EXT_H
#define LIBC_EXT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

double strtod(const char * string, char **endPtr);
double fabs(double n);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LIBC_EXT_H */

