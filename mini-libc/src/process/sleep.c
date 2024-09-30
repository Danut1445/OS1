#include <internal/syscall.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

unsigned int sleep(unsigned int sec)
{
	struct timespec wait = {0};
	wait.tm_sec = sec;
	unsigned int time = nanosleep(&wait, NULL);
	return time;
}
