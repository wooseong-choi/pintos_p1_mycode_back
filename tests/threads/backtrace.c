// backtrace.c

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>

void callstack_dump(void)
{
	void *callstack[128];
	int i, nr_frames;
	char **strs;

	nr_frames = backtrace(callstack, sizeof(callstack)/sizeof(void *));
	strs = backtrace_symbols(callstack, nr_frames);
	for (i = 0; i < nr_frames; i++) {
		printf("%s\n", strs[i]);
	}
	free(strs);
}

void child(void)
{
	callstack_dump();
}

void parent(void)
{
	child();
}

int main(void)
{
	parent();
	return 0;
}