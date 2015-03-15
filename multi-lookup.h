#include "queue.h"

typedef struct rarg_s
{
	queue * q;
	FILE * of;
}rarg;

typedef struct rawr_s
{
	queue * q;
	char * filename;
}rawr;

void* request(void *filename);

void* resolve();
