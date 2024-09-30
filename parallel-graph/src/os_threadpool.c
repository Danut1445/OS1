// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t;

	t = malloc(sizeof(*t));
	DIE(t == NULL, "malloc");

	t->action = action;		// the function
	t->argument = arg;		// arguments for the function
	t->destroy_arg = destroy_arg;	// destroy argument function

	return t;
}

/* Destroy task. */
void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);

	/* TODO: Enqueue task to the shared task queue. Use synchronization. */
	pthread_mutex_lock(&(tp->statusqueue));
	list_add_tail(&(tp->head), &(t->list));
	tp->nrtasks++;
	tp->started = 1;
	pthread_cond_signal(&(tp->doWork));
	pthread_mutex_unlock(&(tp->statusqueue));
}

/*
 * Check if queue is empty.
 * This function should be called in a synchronized manner.
 */
static int queue_is_empty(os_threadpool_t *tp)
{
	return list_empty(&tp->head);
}

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */

os_task_t *dequeue_task(os_threadpool_t *tp)
{
	/* TODO: Dequeue task from the shared task queue. Use synchronization. */

	pthread_mutex_lock(&(tp->statusqueue));

	//printf("Am intrat in deque\n");

	while (queue_is_empty(tp)) {
		tp->waiting++;
		if (tp->waiting == tp->num_threads && tp->started == 1) {
			pthread_cond_broadcast(&(tp->doWork));
			pthread_mutex_unlock(&(tp->statusqueue));
			return NULL;
		}
		pthread_cond_wait(&(tp->doWork), &(tp->statusqueue));
		tp->waiting--;
	}

	//printf("Am trecut de wait\n");

	if (queue_is_empty(tp)) {
		pthread_mutex_unlock(&(tp->statusqueue));
		return NULL;
	}
	os_list_node_t *currNode = tp->head.next;

	//printf("Am gasit un element\n");

	list_del(currNode);
	tp->nrtasks--;
	pthread_mutex_unlock(&(tp->statusqueue));
	os_task_t *currTask = ((void *) currNode) - 24;
	return currTask;
}

/* Loop function for threads */
static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *) arg;

	while (1) {
		os_task_t *t;

		t = dequeue_task(tp);

		if (t == NULL)
			break;

		t->action(t->argument);
		destroy_task(t);
	}

	pthread_cond_broadcast(&(tp->doWork));
	return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
	/* TODO: Wait for all worker threads. Use synchronization. */

	/* Join all worker threads. */
	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = NULL;
	int rc;

	tp = malloc(sizeof(*tp));
	DIE(tp == NULL, "malloc");

	list_init(&tp->head);

	/* TODO: Initialize synchronization data. */
	pthread_mutex_init(&(tp->statusqueue), NULL);
	pthread_cond_init(&(tp->doWork), NULL);
	pthread_cond_init(&(tp->doStart), NULL);
	tp->waiting = 0;
	tp->nrtasks = 0;
	tp->started = 0;
	tp->finished = 0;
	tp->currentNr = num_threads;

	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(*tp->threads));
	DIE(tp->threads == NULL, "malloc");
	for (unsigned int i = 0; i < num_threads; ++i) {
		rc = pthread_create(&tp->threads[i], NULL, &thread_loop_function, (void *) tp);
		DIE(rc < 0, "pthread_create");
	}

	return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;

	/* TODO: Cleanup synchronization data. */
	pthread_mutex_destroy(&(tp->statusqueue));
	pthread_cond_destroy(&(tp->doWork));
	pthread_cond_destroy(&(tp->doStart));

	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	free(tp->threads);
	free(tp);
}
