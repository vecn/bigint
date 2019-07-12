#ifndef _BIG_INT_H_
#define _BIG_INT_H

#include <stdint.h>

typedef struct bigint_s bigint_t;

bigint_t *bigint_create(uint32_t words);
bigint_t *bigint_clone(const bigint_t *src);
void bigint_destroy(bigint_t *big);

void bigint_set_u32(bigint_t *big, uint32_t a);
void bigint_set_u64(bigint_t *big, uint64_t a);
void bigint_set_hexadec(bigint_t *big, const char *hexadec);
void bigint_set_decimal(bigint_t *big, const char *decimal, bigint_t *aux);
void bigint_copy(bigint_t *big, const bigint_t *src);
int bigint_is_zero(const bigint_t *big);
int bigint_gt(const bigint_t *big, uint32_t gt);
void bigint_set_max(bigint_t *big);
void bigint_set_lsbit(bigint_t *big);
int bigint_get_lsbit(const bigint_t *big);
uint32_t bigint_count_on_bits(const bigint_t *big);
uint32_t bigint_index_of_msbit(const bigint_t *big);
int bigint_is_2k(const bigint_t *big);
int bigint_get_word(const bigint_t *big, int i, uint32_t *word);
void bigint_set_word(bigint_t *big, int i, uint32_t word);
int bigint_compare_u32(const bigint_t *big, uint32_t n);
int bigint_compare_u64(const bigint_t *big, uint64_t n);
int bigint_compare_2k(const bigint_t *big, uint32_t bit);
int bigint_compare_2kless1(const bigint_t *big, uint32_t k);
int bigint_compare(const bigint_t *a, const bigint_t *b);
uint32_t bigint_get_2k_geq(const bigint_t *big);
uint32_t bigint_truncate_u32(const bigint_t *big);
uint64_t bigint_truncate_u64(const bigint_t *big);
void bigint_add_u32(bigint_t *big, uint32_t add);
void bigint_add_u64(bigint_t *big, uint64_t add);
void bigint_add_2k(bigint_t *big, uint32_t bit);
void bigint_add(bigint_t *big, const bigint_t *add);
void bigint_increment(bigint_t *big);
void bigint_subtract_u32(bigint_t *big, uint32_t num, int *status);
void bigint_subtract_2k(bigint_t *big, uint32_t bit, int *status);
void bigint_subtract(bigint_t *big, const bigint_t *num, int *status);
void bigint_decrement(bigint_t *big);
void bigint_shift_left(bigint_t *big, uint32_t n);
void bigint_shift_right(bigint_t *big, uint32_t n);
void bigint_mul_u32(const bigint_t *big, uint32_t x, bigint_t *result);
void bigint_mul_u64(const bigint_t *big, uint64_t x, bigint_t *result);
void bigint_mul_2k(bigint_t *big, uint32_t bit);
void bigint_mul(bigint_t *big, const bigint_t *x, bigint_t *aux);
void bigint_mul_fast(bigint_t *big, const bigint_t *x,
		     bigint_t *aux1, bigint_t *aux2);
void bigint_div_u32(bigint_t *big, uint32_t div, uint32_t *res);
void bigint_div_u64(bigint_t *big, uint64_t div, uint64_t *res);
void bigint_div_2k(bigint_t *big, uint32_t k);
void bigint_div(bigint_t *big, const bigint_t *div, bigint_t *res);
void bigint_div_2kless1(bigint_t *big, uint32_t k, bigint_t *res);
void bigint_div_fast(bigint_t *big, const bigint_t *div, bigint_t *res,
	             bigint_t *aux1, bigint_t *aux2, bigint_t *aux3);
void bigint_mod_2k(bigint_t *big, uint32_t k);
void bigint_mod_2kless1(bigint_t *big, uint32_t k);
void bigint_mod(bigint_t *big, const bigint_t *div,
		bigint_t *aux1, bigint_t *aux2, bigint_t *aux3);
void bigint_get_binary_string(const bigint_t *big, char *str);
void bigint_get_hexadec_string(const bigint_t *big, char *str);
void bigint_get_decimal_string(const bigint_t *big, char* str);
void bigint_pow(bigint_t *big, uint32_t p, bigint_t *aux);
void bigint_pow_fast(bigint_t *big, uint32_t p, bigint_t *aux1, bigint_t *aux2);
void bigint_sqrt(bigint_t *big, bigint_t *res);
  
#endif
