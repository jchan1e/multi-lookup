/*
 * File: lookup.c
 * Author: Andy Sayler
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2012/02/01
 * Modify Date: 2012/02/01
 * Description:
 * 	This file contains the reference non-threaded
 *      solution to this assignment.
 *  
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
//#include <sys/sysinfo.h>

#include "util.h"
#include "multi-lookup.h"
#include <pthread.h>

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"


pthread_mutex_t mPush = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mPop = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mQueue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mOut = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t eBrake;

int main(int argc, char* argv[]){

	/* Local Vars */
	FILE* outputfp = NULL;
	int num_cores = /*get_nprocs()*/ 8;

	/* Check Arguments */
	if(argc < MINARGS){
		fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
	}

	/* Open Output File */
	outputfp = fopen(argv[(argc-1)], "w");
	if(!outputfp){
		perror("Error: Check Output File Path");
		return EXIT_FAILURE;
	}

	/* Pass Input Files Through Requester Threads */

	int NUM_THREADS = argc-2;
	pthread_t threads[NUM_THREADS];
	char* filename[NUM_THREADS];

	queue q;
	if (queue_init(&q, 8) == QUEUE_FAILURE)
		fprintf(stderr, "error: failed to create queue\n");


	int i;
	for (i = 0; i < argc - 2; ++i)
	{
		filename[i] = argv[i+1];
		printf("%s\n", filename[i]);
		rawr *args = malloc(sizeof(rawr));
		args->q = &q;
		args->filename = filename[i];
		pthread_create(&threads[i], NULL, &request, args);
	}

	/* spin up resolver threds */

	rarg* arg = malloc(sizeof(rarg));
	arg->q = &q;
	arg->of = outputfp;

	pthread_t moreThreads[num_cores];

	for (i = 0; i < num_cores; ++i)
	{
		pthread_create(&moreThreads[i], NULL, &resolve, arg);
	}

	for (i = 0; i < argc-2; ++i)
		pthread_join(threads[i], NULL);

	q.readOnly = 1;

	for (i = 0; i < num_cores; ++i)
		pthread_join(moreThreads[i], NULL);

	/* Close Output File */
	fclose(outputfp);

	return EXIT_SUCCESS;
}



void* request(void *args)
{
	rawr *rargs = (rawr*)args;
	queue *q = rargs->q;
	char* fn = rargs->filename;
	free(args);
	printf("request started %s\n", fn);

	FILE* file = fopen(fn, "r");
	if (file == NULL)
		exit(EXIT_FAILURE);
	int n = 1;
	char *domain = NULL;
	while (n > 0)
	{
		domain = malloc(SBUFSIZE * sizeof(char));
		n = fscanf(file, "%1024s", domain);
		pthread_mutex_lock(&mPush); printf("PUSH lock %s\n", fn);
		pthread_mutex_lock(&mQueue); printf("QUEUE lock %s\n", fn);
		while(queue_is_full(q))
		{
			printf("WAITING On full queue %s\n", fn); 
			pthread_cond_wait(&eBrake, &mQueue); 
			printf("SIGNALELED, no longer waiting %s\n", fn);
		}
		if (queue_is_empty(q))
		{
			queue_push(q, domain);
			pthread_cond_signal(&eBrake); printf("SIGNALLING %s\n", fn);
		}
		else
			queue_push(q, domain);
		printf("pushing %s from %s\n", domain, fn);
		pthread_mutex_unlock(&mQueue); printf("QUEUE unlock %s\n", fn);
		pthread_mutex_unlock(&mPush); printf("PUSH unlock %s\n", fn);
	}
	printf("closing %s\n", fn);
	fclose(file);
	pthread_exit(NULL);
}

void* resolve(void *arg)
{
	printf("resolver thread started\n");
	rarg *blarg = (rarg*)arg;
	queue *q = blarg->q;
	FILE* output = blarg->of;

	char* domain = NULL;
	char ipstr[INET6_ADDRSTRLEN];

	while(!q->readOnly)
	{
		pthread_mutex_lock(&mPop); printf("POP lock %s\n", domain);
		pthread_mutex_lock(&mQueue); printf("QUEUE lock %s\n", domain);
		while(queue_is_empty(q) && !q->readOnly)
		{
			printf("WAITING on enpty queue %s\n", domain);
	  		pthread_cond_wait(&eBrake, &mQueue);
			printf("SIGNALLED, no longer waiting %s\n", domain);
		}
		if (!q->readOnly)
		{
			if (queue_is_full(q))
			{
				domain = queue_pop(q);
				pthread_cond_signal(&eBrake); printf("SIGNALLING %s\n", domain);
			}
			else
				domain = queue_pop(q);
		}
		pthread_mutex_unlock(&mQueue); printf("QUEUE unlock %s\n", domain);
		pthread_mutex_unlock(&mPop); printf("POP unlock %s\n", domain);

//		if (dnslookup(domain, ipstr, sizeof(ipstr)) == UTIL_FAILURE)
//			fprintf(stderr, "dnslookup error: %s\n", domain);
//		strncpy(ipstr, "", sizeof(ipstr));
//		
//		pthread_mutex_lock(&mOut);
//		fprintf(output, "%s, %s\n", domain, ipstr);
//		pthread_mutex_unlock(&mOut);

		printf("%s\n", domain);

		free(domain);
	}
	printf("Hey, the queue's read-only now!\n");
	int m = 1;
	while(!m)
	{
		pthread_mutex_lock(&mPop);
		pthread_mutex_lock(&mQueue);
		m = queue_is_empty(q);
		if (!m)
			domain = queue_pop(q);
		pthread_mutex_unlock(&mQueue);
		pthread_mutex_unlock(&mPop);

		printf("%s\n", domain);
		free(domain);
	}


	printf("resolver exiting %s\n", domain);
	pthread_exit(NULL);
}


