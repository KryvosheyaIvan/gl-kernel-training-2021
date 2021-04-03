// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define BASE 2
#define POW_LIMIT 64

#define SAMPLE_NUM 10

#define NUM_RESET_MIN_VAL	1e9
#define NUM_RESET_MAX_VAL	-1.0
#define NUM_RESET_AVG_VAL	0.0

#define RESET_STATS	true
#define COUNT_STATS	false

#define RECOUNT_ALLOC	true
#define RECOUNT_FREE 	false

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
	double free_time_min;
	double free_time_max;
	double free_time_avg;
	double alloc_time_min;
	double alloc_time_max;
	double alloc_time_avg;
} m_malloc[POW_LIMIT], m_calloc[POW_LIMIT], m_alloca[POW_LIMIT];

static bool test_alloc_heap_data(int power);
static bool test_print_alloc_stack_data(int power);
static void print_heap_timings(int len);
static double elapsed_time(void);

static double recount_min(double act, bool reset, bool is_alloc);
static double recount_max(double act, bool reset, bool is_alloc);
static double recount_avg(double act, bool reset, bool is_alloc);

#define RESET_ALL_STATS_VALUES()\
			recount_min( 0.0,  RESET_STATS, RECOUNT_ALLOC);\
			recount_max( 0.0,  RESET_STATS, RECOUNT_ALLOC);\
			recount_avg( 0.0,  RESET_STATS, RECOUNT_ALLOC);\
			recount_min( 0.0,  RESET_STATS, RECOUNT_FREE);\
			recount_max( 0.0,  RESET_STATS, RECOUNT_FREE);\
			recount_avg( 0.0,  RESET_STATS, RECOUNT_FREE);\

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
	printf("\t\t      alloc\t\t\t\t\t\t\t      free\n");
	printf("\tmin           avg           max\t\t\t\t\tmin           avg            max\n");
	for (i = 0; i < len; ++i) {
		printf("%d\t%.10f  %.10f  %.10f\t\t\t%.10f  %.10f  %.10f\n",
		       i, m_malloc[i].alloc_time_min,
		       m_malloc[i].alloc_time_avg, m_malloc[i].alloc_time_max,
		       m_malloc[i].free_time_min, m_malloc[i].free_time_avg,
		       m_malloc[i].free_time_max);
	}

	printf("\nCALLOC timings:\n");
	printf("\t\t      alloc\t\t\t\t\t\t\t      free\n");
	printf("\tmin           avg           max\t\t\t\t\tmin           avg            max\n");
	for (i = 0; i < len; ++i) {
		printf("%d\t%.10f  %.10f  %.10f\t\t\t%.10f  %.10f  %.10f\n",
		       i, m_calloc[i].alloc_time_min,
		       m_calloc[i].alloc_time_avg, m_calloc[i].alloc_time_max,
		       m_calloc[i].free_time_min, m_calloc[i].free_time_avg,
		       m_calloc[i].free_time_max);
	}
}

static double recount_min(double act, bool reset, bool is_alloc)
{
	static double min_alloc, min_free;

	if (reset) {
		min_alloc = NUM_RESET_MIN_VAL;
		min_free = NUM_RESET_MIN_VAL;
		return NUM_RESET_MIN_VAL;
	}

	if (is_alloc == RECOUNT_ALLOC) {
		if (act < min_alloc)
			min_alloc = act;
		return min_alloc;
	} else if (is_alloc == RECOUNT_FREE) {
		if (act < min_free)
			min_free = act;
		return min_free;
	}

	return 0.0;
}

static double recount_max(double act, bool reset, bool is_alloc)
{
	static double max_alloc, max_free;

	if (reset) {
		max_alloc = NUM_RESET_MAX_VAL;
		max_free = NUM_RESET_MAX_VAL;
		return NUM_RESET_MAX_VAL;
	}

	if (is_alloc == RECOUNT_ALLOC) {
		if (act > max_alloc)
			max_alloc = act;
		return max_alloc;
	} else if (is_alloc == RECOUNT_FREE) {
		if (act > max_free)
			max_free = act;
		return max_free;
	}

	return 0.0;
}

static double recount_avg(double act, bool reset, bool is_alloc)
{
	static double sum_alloc;
	static double act_avg_alloc;
	static uint8_t samples_num_alloc;

	static double sum_free;
	static double act_avg_free;
	static uint8_t samples_num_free;

	if (reset) {
		act_avg_alloc = NUM_RESET_AVG_VAL;
		sum_alloc = 0;
		samples_num_alloc = 0;

		act_avg_free = NUM_RESET_AVG_VAL;
		sum_free = 0;
		samples_num_free = 0;
		return NUM_RESET_AVG_VAL;
	}

	if (is_alloc == RECOUNT_ALLOC) {
		++samples_num_alloc;
		sum_alloc += act;
		act_avg_alloc = sum_alloc / samples_num_alloc;
		return act_avg_alloc;
	} else if (is_alloc == RECOUNT_FREE) {
		++samples_num_free;
		sum_free += act;
		act_avg_free = sum_free / samples_num_free;
		return act_avg_free;
	}

	return 0.0;
}


static bool test_alloc_heap_data(int power)
{
	int idx;
	uint64_t size_bytes = 1;
	double time_temp;

	idx = power;

	/* calculate size as a power of two */
	while (power != 0) {
		size_bytes *= BASE;
		--power;
	}

	//printf("%d:Heap Test Allocation %lu bytes...\n", idx, size_bytes);


	/* malloc */
	RESET_ALL_STATS_VALUES();
	for (int j = 0; j < SAMPLE_NUM; ++j) {
		START_TIMER();
		pData = malloc(size_bytes * sizeof(uint8_t));
		STOP_TIMER();

		if (pData == NULL)
			goto some_err;
		time_temp = elapsed_time();
		m_malloc[idx].alloc_time_avg = recount_avg(time_temp, COUNT_STATS, RECOUNT_ALLOC);
		m_malloc[idx].alloc_time_max = recount_max(time_temp, COUNT_STATS, RECOUNT_ALLOC);
		m_malloc[idx].alloc_time_min = recount_min(time_temp, COUNT_STATS, RECOUNT_ALLOC);

		START_TIMER();
		free(pData);
		STOP_TIMER();
		time_temp = elapsed_time();
		m_malloc[idx].free_time_avg = recount_avg(time_temp, COUNT_STATS, RECOUNT_FREE);
		m_malloc[idx].free_time_max = recount_max(time_temp, COUNT_STATS, RECOUNT_FREE);
		m_malloc[idx].free_time_min = recount_min(time_temp, COUNT_STATS, RECOUNT_FREE);
	}

	/* calloc */
	RESET_ALL_STATS_VALUES();
	for (int j = 0; j < SAMPLE_NUM; ++j) {

		START_TIMER();
		pData = calloc(size_bytes, sizeof(uint8_t));
		STOP_TIMER();

		if (pData == NULL)
			goto some_err;
		time_temp = elapsed_time();
		m_calloc[idx].alloc_time_avg = recount_avg(time_temp, COUNT_STATS, RECOUNT_ALLOC);
		m_calloc[idx].alloc_time_max = recount_max(time_temp, COUNT_STATS, RECOUNT_ALLOC);
		m_calloc[idx].alloc_time_min = recount_min(time_temp, COUNT_STATS, RECOUNT_ALLOC);

		START_TIMER();
		free(pData);
		STOP_TIMER();
		time_temp = elapsed_time();
		m_calloc[idx].free_time_avg = recount_avg(time_temp, COUNT_STATS, RECOUNT_FREE);
		m_calloc[idx].free_time_max = recount_max(time_temp, COUNT_STATS, RECOUNT_FREE);
		m_calloc[idx].free_time_min = recount_min(time_temp, COUNT_STATS, RECOUNT_FREE);
	}

	/* ok */
	return true;

some_err:
	printf("\nFail to allocate %lu bytes...\n", size_bytes);
	free(pData);

	return false;
}

static bool test_print_alloc_stack_data(int power)
{
	int idx;
	uint64_t size_bytes = 1;
	double time_temp = 0;

	idx = power;

	/* calculate size as a power of two */
	while (power != 0) {
		size_bytes *= BASE;
		--power;
	}

	//printf("\n%d:Stack Test Allocation %lu bytes...\n", idx, size_bytes);

	/* alloca */
	RESET_ALL_STATS_VALUES();
	for (int j = 0; j < SAMPLE_NUM; ++j) {
		START_TIMER();
		pData = alloca(size_bytes * sizeof(uint8_t));
		STOP_TIMER();
		if (pData == NULL)
			goto some_err;
		
		m_alloca[idx].alloc_time_avg = recount_avg(time_temp, COUNT_STATS, RECOUNT_ALLOC);
		m_alloca[idx].alloc_time_max = recount_max(time_temp, COUNT_STATS, RECOUNT_ALLOC);
		m_alloca[idx].alloc_time_min = recount_min(time_temp, COUNT_STATS, RECOUNT_ALLOC);
		
		/* no need to free it */
	}

	printf("%d\t%.10f  %.10f  %.10f\t\t\t-             -             -\n", idx, m_malloc[idx].alloc_time_min,
		       m_malloc[idx].alloc_time_avg, m_malloc[idx].alloc_time_max);
	

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

	printf("\nALLOCA timings:\n");
	printf("\t\t      alloc\t\t\t\t\t\t\t      free\n");
	printf("\tmin           avg           max\t\t\t\t\tmin           avg            max\n");
	for (i = 0; i < POW_LIMIT; i++) {
		enough_mem = test_print_alloc_stack_data(i);
		if (!enough_mem)
			break;
	}

	printf("You unlikely to see it\n");

	return 0;
}
