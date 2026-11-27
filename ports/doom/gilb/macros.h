/* macros.h -- token pasting (from the MG24 port's src/macros.h, Nicola Wrachien, GPLv2+). */
#ifndef SRC_MACROS_H_
#define SRC_MACROS_H_
#define CATEXP(a, b)                    a ## b
#define CAT(a, b)                       CATEXP(a, b)
#define CAT3(a,b,c)                     CAT3EXP(a, b, c)
#define CAT3EXP(a,b,c)                  a ## b ## c
#endif
