// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */
static pthread_mutex_t graphwork;
/* TODO: Define graph task argument. */

static void process_node(void *arg)
{
	int curr = *((int *)arg);
	/* TODO: Implement thread-pool based processing of graph. */
	pthread_mutex_lock(&graphwork);
	os_node_t *currNode = graph->nodes[curr];

	sum += currNode->info;
	graph->visited[curr] = DONE;
	pthread_mutex_unlock(&graphwork);

	//printf("%d\n", sum);

	for (unsigned int i = 0; i < currNode->num_neighbours; i++) {
		pthread_mutex_lock(&graphwork);
		if (graph->visited[currNode->neighbours[i]] == NOT_VISITED) {
			int *newarg = malloc(sizeof(int) * 1);

			newarg[0] = currNode->neighbours[i];
			os_task_t *newTask = create_task(&process_node, newarg, &free);

			enqueue_task(tp, newTask);
			graph->visited[currNode->neighbours[i]] = PROCESSING;
		}
		pthread_mutex_unlock(&graphwork);
	}
}

int main(int argc, char *argv[])
{
	FILE *input_file;
	int *curr;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */
	pthread_mutex_init(&graphwork, NULL);
	tp = create_threadpool(NUM_THREADS);
	curr = malloc(sizeof(int));
	curr[0] = 0;
	os_task_t *newTask = create_task(&process_node, curr, &free);

	enqueue_task(tp, newTask);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	return 0;
}
