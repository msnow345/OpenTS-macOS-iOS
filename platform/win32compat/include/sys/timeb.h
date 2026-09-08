#pragma once
#include <sys/time.h>

struct _timeb { long time; unsigned short millitm; short timezone; short dstflag; };
#define timeb _timeb

extern "C" void _ftime(struct _timeb * time);
