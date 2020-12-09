#First copy the files "alarm_cond.c", "errors.h", and "makefile" into the same directory

target: alarm_cond.c
	cc -o alarm_cond alarm_cond.c -D_POSIX_PTHREAD_SEMANTICS -lpthread