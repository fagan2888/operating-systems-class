#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "util.h"
#include "queue.h"
#include "multi-lookup.h"

static const int MIN_ARGS = 3;
static const int MAX_INPUT_FILES = 10;
static const int NUM_RESOLVER_THREADS = 6;
static const int MAX_NAME_LENGTH = 1025;
static const char INPUT_FS[] = "%1024s";

queue q;
pthread_mutex_t lock_queue;

FILE *output_fp;
pthread_mutex_t lock_output_file;

// this variable is used to detect if any requester threads running
// this is useful when the queue is empty -- only when there are no requester threads running
// can a resolver thread exit
int num_active_requesters = 0;
pthread_mutex_t lock_active_requesters;



int main(int argc, char **argv)
{
	if (argc < MIN_ARGS) {
		fprintf(stderr, "Requires at least %d arguments: the executable, one or more input files, and the results filename.\n", MIN_ARGS);
		return EXIT_FAILURE;
	}

	if (argc > 12) {
		fprintf(stderr, "There cannot be more than %d input files. Please try again with less input files.\n", MAX_INPUT_FILES);
		return EXIT_FAILURE;
	}

	char *output_filename = argv[argc-1];
	output_fp = fopen(output_filename, "w");
	if (!output_fp) {
		fprintf(stderr, "Failed to open specified output file.\n");
		return EXIT_FAILURE;
	}

	queue_init(&q, QUEUEMAXSIZE);
	pthread_mutex_init(&lock_queue, NULL);
	pthread_mutex_init(&lock_output_file, NULL);
	pthread_mutex_init(&lock_active_requesters, NULL);

	// one requester thread is created for each input file
	// the resolvers threads will be started in the detatched state, so memory cleanup via join is not necessary for them
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t requester_threads[argc-2];
	for (int i = 1; i < argc-1; i++) {
		increment_requesters();
		pthread_create(&requester_threads[i-1], &attr, requester_entry_point, argv[i]);
	}

	// a static number of resolver threads are created, given by NUM_RESOLVER_THREADS
	pthread_t resolver_threads[NUM_RESOLVER_THREADS];
	for (int i = 0; i < NUM_RESOLVER_THREADS; i++) {
		pthread_create(&resolver_threads[i], NULL, resolver_entry_point, NULL);
	}

	// exiting main exits entire program, so only do so after all threads have completed
	// all requester threads must be complete before resolver threads exit, so we can just check resolver threads
	for (int i = 0; i < NUM_RESOLVER_THREADS; i++) {
		pthread_join(resolver_threads[i], NULL);
	}

	// at this point, the main thread is the only remaining thread, so access to resources does not have to be protected
	fclose(output_fp);
	queue_cleanup(&q);
}

void increment_requesters() {
	pthread_mutex_lock(&lock_active_requesters);
	num_active_requesters++;
	pthread_mutex_unlock(&lock_active_requesters);
}

void decrement_requesters() {
	pthread_mutex_lock(&lock_active_requesters);
	num_active_requesters--;
	pthread_mutex_unlock(&lock_active_requesters);
}

int requesters_are_running() {
	// don't surround this with lock because it is simply a real-time update of value
	return num_active_requesters > 0;
}

void sleep_random()
{
	// sleep randomly between 0 and 100 ms
	int ms_to_sleep = rand() % 101;

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = ms_to_sleep * 1000000;
	nanosleep(&ts, NULL);
}


void *requester_entry_point(void *void_ptr)
{
	char *input_filename = (char *)void_ptr;
	FILE *input_fp = fopen(input_filename, "r");
	if (!input_fp) {
		fprintf(stderr, "Failed to open input file %s.\n", input_filename);
		decrement_requesters();
		return NULL;
	}

	// put hostnames on heap so they're accessible even after hostname is redefined
	char *hostname = malloc(sizeof(char) * MAX_NAME_LENGTH);
	while (fscanf(input_fp, INPUT_FS, hostname) > 0) {
		// access to queue must be protected
		pthread_mutex_lock(&lock_queue);
		while (queue_push(&q, hostname) == QUEUE_FAILURE) {
			pthread_mutex_unlock(&lock_queue);
			sleep_random();
			pthread_mutex_lock(&lock_queue);
		}
		pthread_mutex_unlock(&lock_queue);
		hostname = malloc(sizeof(char) * MAX_NAME_LENGTH);
	}
	// last hostname will not be used in queue, so it can be freed
	free(hostname);

	fclose(input_fp);

	decrement_requesters();
	return NULL;
}

void *resolver_entry_point()
{
	while (1) {
		// access to queue must be protected
		pthread_mutex_lock(&lock_queue);
		char *hostname = (char *)queue_pop(&q);
		pthread_mutex_unlock(&lock_queue);

		if (hostname == NULL) {
			// queue is empty
			// if there are still requesters running, sleep and wait for them to fill up queue
			// otherwise, exit
			if (!requesters_are_running()) {
				return NULL;
			}
			else {
				sleep_random();
			}
		}
		else {
			char ip_str[INET6_ADDRSTRLEN];
			if (dnslookup(hostname, ip_str, sizeof(ip_str)) == UTIL_FAILURE) {
				printf("DNS lookup error: %s\n", hostname);

				// force the ip string to be empty
				ip_str[0] = '\0';
			}
			
			// write to output file and protect this operation
			pthread_mutex_lock(&lock_output_file);
			fprintf(output_fp, "%s,%s\n", hostname, ip_str);
			pthread_mutex_unlock(&lock_output_file);

			// now that the hostname has been used, we can free the memory
			free(hostname);
		}
	}
}