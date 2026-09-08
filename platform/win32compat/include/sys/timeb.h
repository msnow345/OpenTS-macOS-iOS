#pragma once
#include <sys/time.h>
typedef struct _timeb { long time; unsigned short millitm; short timezone; short dstflag; } _timeb;
