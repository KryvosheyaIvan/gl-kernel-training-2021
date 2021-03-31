// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
//#include <math.h>
#include <time.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define BASE 2
#define POW_LIMIT 64

// Start measuring time
#define START_TIMER() clock_gettime(CLOCK_REALTIME, &begin)

// Stop measuring time and calculate the elapsed time
#define STOP_TIMER() clock_gettime(CLOCK_REALTIME, &end)

enum METHOD {
	MALLOC,
	CALLOC,
	ALLOCA
};

//time
struct timespec begin, end;

uint8_t *pData;

struct alloc_method {
	double free_time;
	double alloc_time;
} m_malloc[POW_LIMIT], m_calloc[POW_LIMIT], m_alloca[POW_LIMIT];

static bool test_alloc_heap_data(int power);
static bool test_alloc_stack_data(int power);
static void print_heap_timings(int len);
static double elapsed_time(void);

static double elapsed_time(void)
{
	long seconds = end.tv_sec - begin.tv_sec;
	long nanoseconds = end.tv_nsec - begin.tv_nsec;
	double elapsed = seconds + nanoseconds*1e-9;
	return elapsed;
}

static void print_heap_timings(int len)
{
	int i;

	printf("\nMALLOC timings:\n");
	printf("\t\tmalloc\t\t\t\tfree\n");
	for (i = 0; i < len; ++i) {
		printf("%d\t\t%.10f\t\t\t%.10f\n", i, m_malloc[i].alloc_time,
		       m_malloc[i].free_time);
	}

	printf("\nCALLOC timings:\n");
	printf("\t\tmalloc\t\t\t\tfree\n");
	for (i = 0; i < len; ++i) {
		printf("%d\t\t%.10f\t\t\t%.10f\n", i, m_calloc[i].alloc_time,
		       m_calloc[i].free_time);
	}
}

static void print_stack_timings(int len)
{
	int i;

	printf("\nALLOCA timings:\n");
	printf("\t\tmalloc\t\t\t\tfree\n");
	for (i = 0; i < len; ++i) {
		printf("%d\t\t%.10f\t\t\t%s\n", i, m_alloca[i].alloc_time,
		       "?");
	}
}

static bool test_alloc_heap_data(int power)
{
	int idx;
	uint64_t size_bytes = 1;

	idx = power;

	/* calculate size as a power of two */
	while (power != 0) {
		size_bytes *= BASE;
		--power;
	}

	printf("%d:Heap Test Allocation %lu bytes...\n", idx, size_bytes);

	/* malloc */
	START_TIMER();
	pData = malloc(size_bytes * sizeof(uint8_t));
	STOP_TIMER();
	if (pData == NULL)
		goto some_err;
	m_malloc[idx].alloc_time = elapsed_time();

	START_TIMER();
	free(pData);
	STOP_TIMER();
	m_malloc[idx].free_time = elapsed_time();

	/* calloc */
	START_TIMER();
	pData = calloc(size_bytes, sizeof(uint8_t));
	STOP_TIMER();
	if (pData == NULL)
		goto some_err;
	m_calloc[idx].alloc_time = elapsed_time();

	START_TIMER();
	free(pData);
	STOP_TIMER();
	m_calloc[idx].free_time = elapsed_time();

	/* ok */
	return true;

some_err:
	printf("\nFail to allocate %lu bytes...\n", size_bytes);
	free(pData);

	return false;
}

static bool test_alloc_stack_data(int power)
{
	int idx;
	uint64_t size_bytes = 1;

	idx = power;

	/* calculate size as a power of two */
	while (power != 0) {
		size_bytes *= BASE;
		--power;
	}

	printf("\n%d:Stack Test Allocation %lu bytes...\n", idx, size_bytes);

	/* malloc */
	START_TIMER();
	pData = alloca(size_bytes * sizeof(uint8_t));
	STOP_TIMER();
	if (pData == NULL)
		goto some_err;
	m_alloca[idx].alloc_time = elapsed_time();

	return true;

some_err:
	return false;
}

int main(void)
{
	int i;
	bool enough_mem;

	/* allocate until system rejects to do it */
	for (i = 0; i < POW_LIMIT; i++) {
		enough_mem = test_alloc_heap_data(i);
		if (!enough_mem)
			break;
	}
	print_heap_timings(i);

	for (i = 0; i < POW_LIMIT; i++) {
		enough_mem = test_alloc_stack_data(i);
		if (!enough_mem)
			break;
	}
	print_stack_timings(i);

	return 0;
}
