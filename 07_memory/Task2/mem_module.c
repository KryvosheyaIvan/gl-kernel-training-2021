// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/coda.h> /* struct timespec */
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/jiffies.h>

MODULE_AUTHOR("Ivan Kryvosheia");
MODULE_DESCRIPTION("Memory module for Linux Kernel ProCamp");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("1.0");

#define BASE 2
#define POW_LIMIT 64

#define SAMPLE_NUM 10

#define NUM_RESET_MIN_VAL	1e9
#define NUM_RESET_MAX_VAL	0
#define NUM_RESET_AVG_VAL	0

#define RESET_STATS	true
#define COUNT_STATS	false

#define RECOUNT_ALLOC	true
#define RECOUNT_FREE	false

#define RESET_ALL_STATS_VALUES()\
			do {\
				recount_min(0, RESET_STATS, RECOUNT_ALLOC);\
				recount_max(0, RESET_STATS, RECOUNT_ALLOC);\
				recount_avg(0, RESET_STATS, RECOUNT_ALLOC);\
				recount_min(0, RESET_STATS, RECOUNT_FREE);\
				recount_max(0, RESET_STATS, RECOUNT_FREE);\
				recount_avg(0, RESET_STATS, RECOUNT_FREE);\
			} while (0)

static const char *TABLE_HEAD = "                ALLOC [ns]                       FREE[ns]\n"
				"pow  min        avg        max        min        avg        max";

/* for future enhancement */
/* struct timespec start_time, end_time, elapsed_time; */
unsigned long ns_start, ns_end, ns_elapsed;

static void test_print_kmalloc(void);
static void test_print_kzalloc(void);
static void test_print_vmalloc(void);

static unsigned long recount_max(unsigned long act, bool reset, bool is_alloc);
static unsigned long recount_min(unsigned long act, bool reset, bool is_alloc);
static unsigned long recount_avg(unsigned long act, bool reset, bool is_alloc);

static void mark_start_time(void);
static void mark_end_time(void);
static void calc_time_elapsed(void);

#define NANOSECS_IN_SEC 1000000000

static void mark_start_time(void)
{
	ns_start = ktime_get_ns();
}

static void mark_end_time(void)
{
	ns_end = ktime_get_ns();
}

static void calc_time_elapsed(void)
{
	ns_elapsed = ns_end - ns_start;
}

static unsigned long recount_min(unsigned long act, bool reset, bool is_alloc)
{
	static unsigned long min_alloc, min_free;

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

	return 0;
}

static unsigned long recount_max(unsigned long act, bool reset, bool is_alloc)
{
	static unsigned long max_alloc, max_free;

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

	return 0;
}

static unsigned long recount_avg(unsigned long act, bool reset, bool is_alloc)
{
	static unsigned long sum_alloc;
	static unsigned long act_avg_alloc;
	static uint8_t samples_num_alloc;

	static unsigned long sum_free;
	static unsigned long act_avg_free;
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

	return 0;
}

static uint64_t calc_size_to_alloc(int power)
{
	uint64_t size_bytes = 1;

	/* calculate size as a power of two */
	while (power != 0) {
		size_bytes *= BASE;
		--power;
	}

	return size_bytes;
}

static void test_print_kmalloc(void)
{
	uint8_t *data = NULL;
	uint64_t size_bytes;
	unsigned long min_a, max_a, avg_a;
	unsigned long min_f, max_f, avg_f;
	int pow, j;

	/* allocate until system rejects to do it */
	pr_info("\n\n\n%s", "KMALLOC STATS:");
	pr_info("%s", TABLE_HEAD);
	for (pow = 0; pow < POW_LIMIT; pow++) {
		size_bytes = calc_size_to_alloc(pow);

		RESET_ALL_STATS_VALUES();
		for (j = 0; j < SAMPLE_NUM; ++j) {
			mark_start_time();
			data = kmalloc(size_bytes, GFP_KERNEL);
			mark_end_time();

			if (!data) {
				pr_info("\nkmalloc() allocation error\n");
				return;
			}

			//time_before(start_time, end_time);
			calc_time_elapsed();

			max_a = recount_max(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);
			min_a = recount_min(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);
			avg_a = recount_avg(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);

			mark_start_time();
			kfree(data);
			mark_end_time();
			calc_time_elapsed();

			max_f = recount_max(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
			min_f = recount_min(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
			avg_f = recount_avg(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
		}

		pr_info("%3d  %09lu  %09lu  %09lu  %09lu  %09lu  %09lu\n",
				pow, min_a, avg_a, max_a,
				min_f, avg_f, max_f);
	}
}

static void test_print_kzalloc(void)
{
	uint8_t *data = NULL;
	uint64_t size_bytes;
	unsigned long min_a, max_a, avg_a;
	unsigned long min_f, max_f, avg_f;
	int pow, j;

	/* allocate until system rejects to do it */
	pr_info("\n\n\n%s", "KZALLOC STATS:");
	pr_info("%s", TABLE_HEAD);
	for (pow = 0; pow < POW_LIMIT; pow++) {
		size_bytes = calc_size_to_alloc(pow);

		RESET_ALL_STATS_VALUES();
		for (j = 0; j < SAMPLE_NUM; ++j) {
			mark_start_time();
			data = kzalloc(size_bytes, GFP_KERNEL);
			mark_end_time();

			if (!data) {
				pr_info("\nkzalloc() allocation error\n");
				return;
			}

			//time_before(start_time, end_time);
			calc_time_elapsed();

			max_a = recount_max(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);
			min_a = recount_min(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);
			avg_a = recount_avg(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);

			mark_start_time();
			kzfree(data);
			mark_end_time();
			calc_time_elapsed();

			max_f = recount_max(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
			min_f = recount_min(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
			avg_f = recount_avg(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
		}

		pr_info("%3d  %09lu  %09lu  %09lu  %09lu  %09lu  %09lu\n",
				pow, min_a, avg_a, max_a,
				min_f, avg_f, max_f);
	}
}

static void test_print_vmalloc(void)
{
	uint8_t *data = NULL;
	uint64_t size_bytes;
	unsigned long min_a, max_a, avg_a;
	unsigned long min_f, max_f, avg_f;
	int pow, j;

	/* allocate until system rejects to do it */
	pr_info("\n\n\n%s", "VMALLOC STATS:");
	pr_info("%s", TABLE_HEAD);
	for (pow = 0; pow < POW_LIMIT; pow++) {
		size_bytes = calc_size_to_alloc(pow);

		RESET_ALL_STATS_VALUES();
		for (j = 0; j < SAMPLE_NUM; ++j) {
			mark_start_time();
			data = vmalloc(size_bytes);
			mark_end_time();

			if (!data) {
				pr_info("\nvmalloc() allocation error\n");
				return;
			}
			//time_before(start_time, end_time);
			calc_time_elapsed();

			max_a = recount_max(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);
			min_a = recount_min(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);
			avg_a = recount_avg(ns_elapsed, COUNT_STATS,
					RECOUNT_ALLOC);

			mark_start_time();
			vfree(data);
			mark_end_time();
			calc_time_elapsed();

			max_f = recount_max(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
			min_f = recount_min(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
			avg_f = recount_avg(ns_elapsed, COUNT_STATS,
					RECOUNT_FREE);
		}

		pr_info("%3d  %09lu  %09lu  %09lu  %09lu  %09lu  %09lu\n",
				pow, min_a, avg_a, max_a,
				min_f, avg_f, max_f);
	}
}

static int __init mem_module_init(void)
{
	pr_info("ProCamp Timer Module inserted\n");

	test_print_kmalloc();

	test_print_kzalloc();

	test_print_vmalloc();

	return 0;
}

static void __exit mem_module_exit(void)
{
	pr_info("ProCamp Memory Module removed\n");
}

module_init(mem_module_init);
module_exit(mem_module_exit);
