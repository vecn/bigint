#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#include "bigint.h"

#define N_EXEC_X_TEST 500000

enum {
	ALLOCATE,
	EXECUTE,
	FREE
};

int test_mul(int action, void **resources);
int test_mul_fast(int action, void **resources);
int test_div_u32(int action, void **resources);
int test_div(int action, void **resources);
int test_div_2kless1(int action, void **resources);
int test_div_fast(int action, void **resources);

static int run_tests(int N, int (*tests[])());
static int can_execute_test(int Ntimes, int (*test)());
static uint64_t get_millis();
static void print_summary(int N, int failed);

int main()
{
	int N = 6;
	int (*tests[6])(int, void**) = {
		test_mul,
		test_mul_fast,
		test_div_u32,
		test_div,
		test_div_2kless1,
		test_div_fast
	};
	int failed = run_tests(N, tests);
	print_summary(N, failed);
	return 0;
}

static int run_tests(int N, int (*tests[])(int, void**))
{
	int failed = 0;
	int i;
	for (i = 0; i < N; i++) {
		if (!can_execute_test(N_EXEC_X_TEST, tests[i])) {
			printf("   Test %i failed\n", i);
			failed ++;
		}
	}
	return failed;
}

static int can_execute_test(int Ntimes, int (*test)(int, void**))
{
	void *resources;
	if (test(ALLOCATE, &resources))
		return 0;

	uint64_t t_start = get_millis();
	int i = 0;	
	while (i < Ntimes) {
		if (test(EXECUTE, &resources)) {
			break;
		}
		i++;
	}
	uint64_t t_finish = get_millis();
	printf(" > Average time: %e ms\n", (double)(t_finish - t_start)/i);
	
	if (test(FREE, &resources))
		return 0;
	
	return i == Ntimes;
}

static uint64_t get_millis()
{
    struct timeval tv;

    if (gettimeofday (&tv, NULL) == 0)
        return (uint64_t) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
    else
        return 0;
}

static void print_summary(int N, int failed)
{
	printf("Tests completed: ");
	if (failed) {
		if (failed == N)
			printf("All failed\n");
		else
			printf("%i failed and %i succeeded\n",
			       failed, N - failed);
	} else {
		printf("All succeeded\n");
	}
}

int test_div_u32(int action, void **resources)
{
	int k = 5;
	int bk = 128;
	uint32_t div = (1UL << k) - 1;
	bigint_t *big;
	uint32_t res;
	
	switch (action) {
	case ALLOCATE:
		big = bigint_create(100);
		*resources = (void*) big;
		return 0;
	case EXECUTE:
		big = (bigint_t*) *resources;
		bigint_set_u32(big, 2);
		bigint_add_2k(big, bk);
		bigint_div_u32(big, div, &res);
		return 0;
	case FREE:
		big = (bigint_t*) *resources;
		bigint_destroy(big);
		return 0;
	}
}

int test_mul(int action, void **resources)
{
	int k = 70;
	int bk = 128;
	bigint_t **res;
	
	switch (action) {
	case ALLOCATE:
		res = malloc(3*sizeof(*res));
		res[0] = bigint_create(100);
		res[1] = bigint_create(100);
		res[2] = bigint_create(100);
		*resources = (void*) res;
		return 0;
	case EXECUTE:
		res = (bigint_t**) *resources;
		bigint_set_u32(res[0], 2);
		bigint_add_2k(res[0], bk);
		bigint_set_u32(res[1], 24);
		bigint_add_2k(res[1], k);
		bigint_mul(res[0], res[1], res[2]);
		return 0;
	case FREE:
		res = (bigint_t**) *resources;
		bigint_destroy(res[0]);
		bigint_destroy(res[1]);
		bigint_destroy(res[2]);
		return 0;
	}
}

int test_mul_fast(int action, void **resources)
{
	int k = 70;
	int bk = 128;
	bigint_t **res;
	
	switch (action) {
	case ALLOCATE:
		res = malloc(4*sizeof(*res));
		res[0] = bigint_create(100);
		res[1] = bigint_create(100);
		res[2] = bigint_create(100);
		res[3] = bigint_create(100);
		*resources = (void*) res;
		return 0;
	case EXECUTE:
		res = (bigint_t**) *resources;
		bigint_set_u32(res[0], 2);
		bigint_add_2k(res[0], bk);
		bigint_set_u32(res[1], 24);
		bigint_add_2k(res[1], k);
		bigint_mul_fast(res[0], res[1], res[2], res[3]);
		return 0;
	case FREE:
		res = (bigint_t**) *resources;
		bigint_destroy(res[0]);
		bigint_destroy(res[1]);
		bigint_destroy(res[2]);
		bigint_destroy(res[3]);
		return 0;
	}
}

int test_div(int action, void **resources)
{
	int k = 70;
	int bk = 128;
	bigint_t **res;
	
	switch (action) {
	case ALLOCATE:
		res = malloc(3*sizeof(*res));
		res[0] = bigint_create(100);
		res[1] = bigint_create(100);
		res[2] = bigint_create(100);
		*resources = (void*) res;
		return 0;
	case EXECUTE:
		res = (bigint_t**) *resources;
		bigint_set_u32(res[0], 2);
		bigint_add_2k(res[0], bk);
		bigint_set_u32(res[1], 0);
		bigint_add_2k(res[1], k);
		bigint_subtract_u32(res[1], 1, NULL);
		bigint_div(res[0], res[1], res[2]);
		return 0;
	case FREE:
		res = (bigint_t**) *resources;
		bigint_destroy(res[0]);
		bigint_destroy(res[1]);
		bigint_destroy(res[2]);
		return 0;
	}
}

int test_div_2kless1(int action, void **resources)
{
	int k = 70;
	int bk = 128;
	bigint_t **res;
	
	switch (action) {
	case ALLOCATE:
		res = malloc(2*sizeof(*res));
		res[0] = bigint_create(100);
		res[1] = bigint_create(100);
		*resources = (void*) res ;
		return 0;
	case EXECUTE:
		res = (bigint_t**) *resources;
		bigint_set_u32(res[0], 2);
		bigint_add_2k(res[0], bk);
		bigint_div_2kless1(res[0], k, res[1]);
		return 0;
	case FREE:
		res = (bigint_t**) *resources;
		bigint_destroy(res[0]);
		bigint_destroy(res[1]);
		return 0;
	}
}

int test_div_fast(int action, void **resources)
{
	int k = 70;
	int bk = 128;
	bigint_t **res;
	
	switch (action) {
	case ALLOCATE:
		res = malloc(6*sizeof(*res));
		res[0] = bigint_create(100);
		res[1] = bigint_create(100);
		res[2] = bigint_create(100);
		res[3] = bigint_create(100);
		res[4] = bigint_create(100);
		res[5] = bigint_create(100);
		*resources = (void*) res;
		return 0;
	case EXECUTE:
		res = (bigint_t**) *resources;
		bigint_set_u32(res[0], 2);
		bigint_add_2k(res[0], bk);
		bigint_set_u32(res[1], 0);
		bigint_add_2k(res[1], k);
		bigint_subtract_u32(res[1], 1, NULL);
		bigint_div_fast(res[0], res[1], res[2], res[3], res[4], res[5]);
		return 0;
	case FREE:
		res = (bigint_t**) *resources;
		bigint_destroy(res[0]);
		bigint_destroy(res[1]);
		bigint_destroy(res[2]);
		bigint_destroy(res[3]);
		bigint_destroy(res[4]);
		bigint_destroy(res[5]);
		return 0;
	}
}
