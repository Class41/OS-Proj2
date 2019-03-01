#ifndef SHARED_H

typedef struct {
	int seconds;
	int nanoseconds;
	int fit;
} Shared;

typedef struct {
	int seconds;
	int nanoseconds;
} Target;

#define SHARED_H
#endif
