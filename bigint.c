#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "bigint.h"

#define NMAX  0xFFFFFFFF
#define SET1 0x80000000
#define BITSXWORD 32
#define BXW_MOD_MASK 31
#define BXW_2K 5
              /* Notice that for any unsigned integer X:
	       *    X / 32 = X >> 5  BXW_2K
               *    X % 32 = X & 31  BXW_MOD_MASK
	       */

#define MAX(a,b) (((a)>(b))?(a):(b))

struct bigint_s {
	uint32_t words;
	uint32_t len;
	uint32_t *bits;
};

static void bigint_duplicate_words(bigint_t *big, uint32_t minw);
static uint32_t hex_to_u32(const char *hex, int len);
static int digit_hex2int(char c);
static uint32_t dec_to_u32(const char *dec, int len, uint32_t *pow10);
static int digit_decimal(char c);
static void bigint_update_len(bigint_t *big);
static uint32_t bigint_get_right_most_on_bit(const bigint_t *big);
static int has_off_bits(const bigint_t *big);
static uint32_t index_of_msbit_in_word(uint32_t word);
static void bigint_add_from_word(bigint_t *big, const bigint_t *add, uint32_t w);
static void add_N_div2k(bigint_t *big, const bigint_t *add, uint32_t k);
static void set_div2k_plus_res2k(bigint_t *big, uint32_t k);
static void bigint_shift_left_bits(bigint_t *big, uint32_t n);
static void bigint_shift_left_words(bigint_t *big, uint32_t n);
static void bigint_shift_right_bits(bigint_t *big, uint32_t n);
static void bigint_shift_right_words(bigint_t *big, uint32_t n);
static char char_dec2hex(int n);
static void reverse_string(char *str, int len);
static uint32_t sqrt_u32(uint32_t n, uint32_t *res);
static uint64_t sqrt_u64(uint64_t n, uint64_t *res);
static uint32_t get_2k_4div_leq(const bigint_t *big);
static void sqrt_ubig(bigint_t *big, bigint_t *res);

bigint_t* bigint_create(uint32_t words)
{
	if  (words < 4)
		words = 4;
	bigint_t *big = (bigint_t*) malloc(sizeof(*big));
	big->len = 0;
	big->words = words;
	big->bits = malloc(words * sizeof(uint32_t));
	memset(big->bits, 0, words * sizeof(uint32_t));
	return big;
}

static void bigint_duplicate_words(bigint_t *big, uint32_t minw)
{
	uint32_t w2 = 2 * big->words;
	while (w2 < minw)
		w2 *= 2;
	uint32_t *aux = malloc(big->words * sizeof(uint32_t));
	memcpy(aux, big->bits, big->words * sizeof(uint32_t));
	free(big->bits);
	big->bits = malloc(w2 * sizeof(uint32_t));
	memcpy(big->bits, aux, big->words * sizeof(uint32_t));
	memset(big->bits + big->words, 0, (w2 - big->words) * sizeof(uint32_t));
	big->words = w2;
	free(aux);
}

void bigint_destroy(bigint_t *big)
{
	if (big->words)
		free(big->bits);
	free(big);
}

bigint_t* bigint_clone(const bigint_t *src)
{
	bigint_t *big = bigint_create(src->words);
	big->len = src->len;
	if (src->len)
		memcpy(big->bits, src->bits, src->len * sizeof(uint32_t));
	return big;
}

void bigint_set_u32(bigint_t *big, uint32_t a)
{
	if (big->len)
		memset(big->bits, 0, big->len * sizeof(uint32_t));

	if (a) {
		big->bits[0] = a;
		big->len = 1;
	} else {
		big->len = 0;
	}
}

void bigint_set_u64(bigint_t *big, uint64_t a)
{
	if (big->len)
		memset(big->bits, 0, big->len * sizeof(uint32_t));

	if (a) {
		big->bits[0] = (uint32_t) a;
		big->len = 1;
		a >>= BITSXWORD;
		if (a) {
			big->bits[1] = (uint32_t) a;
			big->len = 2;
		}
	} else {
		big->len = 0;
	}
}
void bigint_set_hexadec(bigint_t *big, const char *hexadec)
{
	int i = 0;
	bigint_set_u32(big, 0);
	while (1) {
		int len = 0;
		while (hexadec[i + len] != '\0' && len < 7)
			len ++;
		if (!len)
			break;
		
		const char *c = hexadec + i;
		uint32_t d = hex_to_u32(c, len);
		bigint_shift_left(big, 4 * len);
		bigint_add_u32(big, d);
		i += len;
	}
}

static uint32_t hex_to_u32(const char *hex, int len)
{
	uint32_t val = 0;
	for (int i = 0; i < len; i++) {
		val <<= 4;
		val += digit_hex2int(hex[i]);
	}
	return val;
}

static int digit_hex2int(char c)
{
	if (c > 47 && c < 58)
		return c - 48;
	else if (c > 64 && c < 71)
		return 10 + c - 65;
	else if (c > 96 && c < 103)
		return 10 + c - 97;
	else
		return 0; // Report this error
}

void bigint_set_decimal(bigint_t *big, const char *decimal)
{
	int i = 0;
	bigint_set_u32(big, 0);
	while (1) {
		int len = 0;
		while (decimal[i + len] != '\0' && len < 8)
			len ++;
		if (!len)
			break;
		
		const char *c = decimal + i;
		uint32_t pow10 = 1;
		uint32_t d = dec_to_u32(c, len, &pow10);
		bigint_mul_u32(big, pow10);
		bigint_add_u32(big, d);
		i += len;
	}
}

static uint32_t dec_to_u32(const char *dec, int len, uint32_t *pow10)
{
	*pow10 = 1;
	uint32_t val = 0;
	for (int i = 0; i < len; i++) {
		(*pow10) *= 10;
		val *= 10;
		val += digit_decimal(dec[i]);
	}
	return val;
}

static int digit_decimal(char c)
{
	if (c > 47 && c < 58)
		return c - 48;
	else
		return 0; // Report this error
}

void bigint_copy(bigint_t *big, const bigint_t *src)
{
	if (big->words < src->len) {
		free(big->bits);
		big->words = src->words;
		big->bits = malloc(big->words * sizeof(uint32_t));
		memset(big->bits, 0, big->words * sizeof(uint32_t));
	} else {
		memset(big->bits, 0, big->len * sizeof(uint32_t));
	}
	if (src->len)
		memcpy(big->bits, src->bits, src->len * sizeof(uint32_t));
	big->len = src->len;
}

static void bigint_update_len(bigint_t *big)
{
	while (big->len > 0) {
		if (big->bits[big->len - 1])
			break;
		big->len -= 1;
	}
}

static uint32_t bigint_get_right_most_on_bit(const bigint_t *big)
{
	if (big->len) {
		uint32_t ix = 0;
		while (!big->bits[ix])
			ix ++;
	
		uint32_t w = big->bits[ix];
		ix = ix << BXW_2K;
		while (!(w & 1)) {
			ix ++;
			w >>= 1;
		}
		return ix;
	} else {
		return 0;
	}
}

int bigint_is_zero(const bigint_t *big)
{
	return big->len == 0;
}

int bigint_gt(const bigint_t *big, uint32_t gt)
{
	// @vecn: CHECK THIS FUNCTION
	if (big->len > 1)
		return 1;
	else if (big->bits[0] > gt)
		return 1;
	else
		return 0;
}

void bigint_set_max(bigint_t *big)
{
	memset(big->bits, 0xFF, big->words * sizeof(uint32_t));
	big->len = big->words;
}

void bigint_set_lsbit(bigint_t *big)
{
	if (big->len == 0)
		big->len = 1;

	big->bits[0] |= 1;
}

int bigint_get_lsbit(const bigint_t *big)
{
	if (big->len == 0)
		return 0;
	else
		return (big->bits[0] & 1);
}

uint32_t bigint_count_on_bits(const bigint_t *big)
{
	uint32_t count = 0;
	int h[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
	for (int i = 0; i < big->len; i++) {
		uint32_t word = big->bits[i];
		while (word) {
			count += h[word & 15];
			word >>= 4;
		}
	}
	return count;
}

int bigint_is_2k(const bigint_t *big)
{
	uint32_t count = 0;
	int h[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
	for (int i = 0; i < big->len; i++) {
		uint32_t word = big->bits[i];
		while (word) {
			count += h[word & 15];
			word >>= 4;
		}
		if (count > 1)
			break;
	}
	return (count > 1) ? 0 : 1;
}

int bigint_get_word(const bigint_t *big, int i, uint32_t *word)
{
	if (i < big->len) {
		*word = big->bits[i];
		return 1;
	} else {
		*word = 0;
		return 0;
	}
}

void bigint_set_word(bigint_t *big, int i, uint32_t word)
{
	if (i >= big->words)
		bigint_duplicate_words(big, i + 1);
	
	big->bits[i] = word;
	if (word) {
	    if (big->len < i + 1)
		big->len = i + 1;
	} else {
	    if (big->len == i + 1)
		bigint_update_len(big);
	}
}

int bigint_compare_u32(const bigint_t *big, uint32_t n)
{
	if (big->len > 1)
		return 1;

	if (big->len == 1) {
		if (big->bits[0] > n)
			return 1;
		else if (big->bits[0] < n)
			return -1;
		else
			return 0;
	}
	return 0;
}
    
int bigint_compare_u64(const bigint_t *big, uint64_t n)
{
	if (big->len > 2)
		return 1;

	if (big->len == 2) {
		uint32_t aux = (n >> BITSXWORD);
		if (big->bits[1] > aux) {
			return 1;
		} else if (big->bits[1] < aux) {
			return -1;
		} else {
			aux = n & NMAX;
			if (big->bits[0] > aux)
				return 1;
			else if (big->bits[0] < aux)
				return -1;
			else
				return 0;
		}
	}
	if (big->len < 2) {
		uint32_t aux = (n >> BITSXWORD);
		if (aux > 0)
			return -1;
		else
			return bigint_compare_u32(big, n & NMAX);

	}
}

int bigint_compare_2k(const bigint_t *big, uint32_t bit)
{
	if (big->len == 0)
		return -1;
	
	uint32_t aux = 1 + (bit >> BXW_2K); /* aux <- 2^k length */
	if (big->len < aux) {
		return -1;
	} else if (big->len > aux) {
		return 1;
	} else {
		bit -= (bit >> BXW_2K) << BXW_2K;
		uint32_t bit2 = 0;
		aux = big->bits[big->len - 1]; /* aux <- last word */
		while (aux) {
			bit2 ++;
			aux >>= 1;
		}
		bit2 --; /* bit2 is always > 0 after previous cycle*/
		if (bit2 < bit) {
			return -1;
		} else if (bit2 > bit) {
			return 1;
		} else {
			/* aux <- On-bits count */
			aux = bigint_count_on_bits(big);
			if (aux > 1)
				return 1;
			else
				return 0;
		}
	}
}

int bigint_compare_2kless1(const bigint_t *big, uint32_t k)
{
	uint32_t bk = bigint_index_of_msbit(big);
	if (k - 1 < bk) {
		return 1;
	} else if (k - 1 > bk) {
		return -1;
	} else {
		return has_off_bits(big) ? (-1) : 0;
	}
}

static int has_off_bits(const bigint_t *big)
{
	/* Return 1 if binary contains at least one zero
	 * Return 0 if binary is only made of one bits (2^k-1)
	 */
	if (big->len == 0)
		return 1;
	
	uint32_t i = 0;
	for (i = 0; i < big->len - 1; i ++) {
		if (big->bits[i] < NMAX)
			return 1;
	}
	uint32_t word = big->bits[i];
	while (word) {
		if (!(word & 1U))
			return 1;
		word >>= 1;
	}
	return 0;
}

int bigint_compare(const bigint_t *a, const bigint_t *b)
{
	if (a->len > b->len) {
		return 1;
	} else if (a->len < b->len) {
		return -1;
	} else {
		for (int i = a->len - 1; i >= 0; i--) {
			if (a->bits[i] > b->bits[i])
				return 1;
			else if (a->bits[i] < b->bits[i])
				return -1;
		}
		return 0;
	}
}

static uint32_t index_of_msbit_in_word(uint32_t word)
{
	if (word) {
		uint32_t byte = 1;
		while ((uint64_t)word >> (byte << 3)) {
			byte ++;
		}
		int bit = (byte - 1) << 3;
		while ((uint64_t)word >> bit) {
			bit ++;
		}
		return bit - 1;
	} else {
		return 0;
	}
}

uint32_t bigint_index_of_msbit(const bigint_t *big)
{
	if (big->len) {
		uint32_t msbit = (big->len - 1) << BXW_2K;
		uint32_t word = big->bits[big->len - 1];
		return msbit + index_of_msbit_in_word(word);
	} else {
		return 0;
	}
}

uint32_t bigint_get_2k_geq(const bigint_t *big)
{
	uint32_t msbit =  bigint_index_of_msbit(big);
	return bigint_is_2k(big) ? msbit : (msbit + 1);
}

uint32_t bigint_truncate_u32(const bigint_t *big)
{
	if (big->len > 0) {
		return big->bits[0];
	} else {
		return 0;
	}
}

uint64_t bigint_truncate_u64(const bigint_t *big)
{
	if (big->len > 1) {
		uint64_t n = ((uint64_t) big->bits[1]) << BITSXWORD;
		n |= ((uint64_t) big->bits[0]);
		return n;
	} else {
		return bigint_truncate_u32(big);
	}
}

void bigint_add_u32(bigint_t *big, uint32_t add)
{
	uint64_t sum = add;
	uint32_t len = big->len;
	for (int i = 0; i < len; i++) {
		sum += (uint64_t) big->bits[i];
		big->bits[i] = (uint32_t) sum;
		sum >>= BITSXWORD;
		if (!sum)
			break;
	}
	if (sum) {
		if (len >= big->words)
			bigint_duplicate_words(big, len);
		big->bits[len] = sum;
		len ++;
	}
	big->len = len;
}

void bigint_add_u64(bigint_t *big, uint64_t add)
{
	uint64_t sum = (uint32_t) add;
	uint32_t sum_aux = (uint32_t) (add >> BITSXWORD);
	uint32_t len = big->len;
	for (int i = 0; i < len; i++) {
		sum += (uint64_t) big->bits[i];
		if (i == 1)
			sum += sum_aux;
		big->bits[i] = (uint32_t) sum;
		sum >>= BITSXWORD;
		if (!sum)
			break;
	}
	if (sum) {
		if (len >= big->words)
			bigint_duplicate_words(big, len);
		big->bits[len] = sum;
		len ++;
	}
	big->len = len;
}

void bigint_add_2k(bigint_t *big, uint32_t bit)
{
	uint32_t word = bit >> BXW_2K;
	bit -= (word << BXW_2K);
	uint64_t sum = 1;
	sum <<= bit;
	
	uint32_t len = MAX(big->len, word + 1);
	if (len > big->words)
		bigint_duplicate_words(big, len);
	
	for (int i = word; i < len; i++) {
		if (i < big->len)
			sum += (uint64_t) big->bits[i];
		big->bits[i] = (uint32_t) sum;
		sum >>= BITSXWORD;
	}
	if (sum) {
		if (len >= big->words)
			bigint_duplicate_words(big, len + 1);
		big->bits[len] = sum;
		len ++;
	}
	big->len = len;
}

static void bigint_add_from_word(bigint_t *big, const bigint_t *add, uint32_t w)
{
	uint32_t len = MAX(big->len, add->len);
	if (len > big->words)
		bigint_duplicate_words(big, len);

	uint64_t sum = 0;
	for (int i = w; i < len; i++) {
		if (i < add->len) {
			sum += (uint64_t) big->bits[i] + add->bits[i];
		} else {
			if (!sum)
				break;
			sum += (uint64_t) big->bits[i];
		}
		big->bits[i] = (uint32_t) sum;
		sum >>= BITSXWORD;
	}
	if (sum) {
		if (len >= big->words)
			bigint_duplicate_words(big, len + 1);
		big->bits[len] = sum;
		len ++;
	}
	big->len = len;
}

void bigint_add(bigint_t *big, const bigint_t *add)
{
	bigint_add_from_word(big, add, 0);
}

void bigint_increment(bigint_t *big)
{
	bigint_add_u32(big, 1);
}

void bigint_subtract_u32(bigint_t *big, uint32_t num)
/* Big must be greater or equal than num */
{
	uint64_t borrow = (uint64_t) big->bits[0] - num;
	big->bits[0] = (uint32_t) borrow;
	borrow = (borrow >> BITSXWORD) & 1; 
	for (int i = 1; i < big->len && borrow; i++) {
		borrow = (uint64_t) big->bits[i] - borrow;
		big->bits[i] = (uint32_t) borrow;
		borrow = (borrow >> BITSXWORD) & 1; 
	}
	bigint_update_len(big);
}

void bigint_subtract_2k(bigint_t *big, uint32_t bit)
/* Big must be greater or equal than 2^(bit) */
{
	uint32_t word = bit >> BXW_2K;
	bit -= (word << BXW_2K);	
	uint64_t borrow = 1;
	borrow <<= bit;
	
	for (int i = word; i < big->len; i++) {
		borrow = (uint64_t) big->bits[i] - borrow;
		big->bits[i] = (uint32_t) borrow;
		borrow = (borrow >> BITSXWORD) & 1; 
	}
	bigint_update_len(big);
}

void bigint_subtract(bigint_t *big, const bigint_t *num)
/* Big must be greater or equal than num */
{
	uint64_t borrow = 0;
	for (int i = 0; i < big->len; i++) {
		if (i < num->len)
			borrow = (uint64_t) big->bits[i] - num->bits[i] - borrow;
		else
			borrow = (uint64_t) big->bits[i] - borrow;
		big->bits[i] = (uint32_t) borrow;
		borrow = (borrow >> BITSXWORD) & 1; 
	}
	bigint_update_len(big);
}

void bigint_decrement(bigint_t *big)
{
	if (big->len > 0) {    
		int i = 0;
		while (i < big->len) {
			if (big->bits[i])
				break;
			big->bits[i] = NMAX;
			i++;
		}
		big->bits[i] -= 1;
		if (!big->bits[i])
			big->len -= 1;
	}
}

static void bigint_shift_left_bits(bigint_t *big, uint32_t n)
{
	if (n > 0 && big->len > 0) {
		uint32_t len = big->len;    
		for (uint32_t i = len; i > 0; i--) {
			big->bits[i] |= big->bits[i - 1] >> (BITSXWORD - n);
			big->bits[i - 1] <<= n;
		}
		if (big->bits[len])
			big->len = len + 1;
	}
}

static void bigint_shift_left_words(bigint_t *big, uint32_t n)
{
	if (n > 0 && big->len > 0) {
		if (big->len > 100) {
			uint32_t *aux = malloc(big->len * sizeof(uint32_t));
			memcpy(aux, big->bits, big->len * sizeof(uint32_t));
			memset(big->bits, 0, n * sizeof(uint32_t));
			memcpy(big->bits + n, aux, big->len * sizeof(uint32_t));
			big->len += n;
			free(aux);
		} else {
			uint32_t aux[100];
			memcpy(aux, big->bits, big->len * sizeof(uint32_t));
			memset(big->bits, 0, n * sizeof(uint32_t));
			memcpy(big->bits + n, aux, big->len * sizeof(uint32_t));
			big->len += n;
		}
	}
}

void bigint_shift_left(bigint_t *big, uint32_t n)
{
	if (n > 0) {
		uint32_t w = n >> BXW_2K;
		int req_len = 1 + w + big->len;
		if (req_len > big->words)
			bigint_duplicate_words(big, req_len);

		bigint_shift_left_words(big, w);
		bigint_shift_left_bits(big, n & BXW_MOD_MASK);
	}
}

static void bigint_shift_right_bits(bigint_t *big, uint32_t n)
{   
	if (n > 0 && big->len > 0) {
		uint32_t len = big->len;
		big->bits[0] >>= n;
    
		for (uint32_t i = 0; i < len - 1; i++) {
			big->bits[i] |= big->bits[i + 1] << (BITSXWORD - n);
			big->bits[i + 1] >>= n;
		}

		if (!(big->bits[len-1]))
			big->len = len - 1;
	}
}

static void bigint_shift_right_words(bigint_t *big, uint32_t n)
{
	if (n > 0 && big->len > 0) {
		if (n < big->len) {
			uint32_t aux_len = (big->len - n);
			if (aux_len > 100) {
				uint32_t *aux = malloc(aux_len * sizeof(uint32_t));
				memcpy(aux, big->bits + n, aux_len * sizeof(uint32_t));
				memcpy(big->bits, aux, aux_len * sizeof(uint32_t));
				memset(big->bits + aux_len, 0, n * sizeof(uint32_t));
				big->len -= n;
				free(aux);
			} else {
				uint32_t aux[100];
				memcpy(aux, big->bits + n, aux_len * sizeof(uint32_t));
				memcpy(big->bits, aux, aux_len * sizeof(uint32_t));
				memset(big->bits + aux_len, 0, n * sizeof(uint32_t));
				big->len -= n;
			}
		} else {
			big->len = 0;
			memset(big->bits, 0, big->len * sizeof(uint32_t));
		}
	}
}

void bigint_shift_right(bigint_t *big, uint32_t n)
{
	bigint_shift_right_words(big, n >> BXW_2K);
	bigint_shift_right_bits(big, n & BXW_MOD_MASK);
}

void bigint_mul_u32(bigint_t *big, uint32_t x)
{
	if (!bigint_is_zero(big)) {
		if (!x) {
			bigint_set_u32(big, 0);
		} else {
			bigint_t *init = bigint_clone(big);
			if (!(x & 1))
				bigint_set_u32(big, 0);
	    
			int shift_bits = 0;
			for (uint32_t i = 1; i < BITSXWORD - 1; i++) {
				if (x & (1 << i)) {
					bigint_shift_left(init, i - shift_bits);
					shift_bits = i;
					bigint_add(big, init);
				}
			}
			bigint_destroy(init);
		}
	}
}

void bigint_mul_u64(bigint_t *big, uint64_t x)
{
	if (!bigint_is_zero(big)) {
		if (!x) {
			bigint_set_u32(big, 0);
		} else {
			bigint_t *init = bigint_clone(big);
			if (!(x & 1))
				bigint_set_u32(big, 0);
	    
			int shift_bits = 0;
			for (uint32_t i = 1; i < (2 << BXW_2K) - 1; i++) {
				if (x & ((uint64_t)1 << i)) {
					bigint_shift_left(init, i - shift_bits);
					shift_bits = i;
					bigint_add(big, init);
				}
			}
			bigint_destroy(init);
		}
	}
}

void bigint_mul_2k(bigint_t *big, uint32_t k)
{
	bigint_shift_left(big, k);
}

void bigint_mul(bigint_t *big, const bigint_t *x, bigint_t *aux)
{
	/* Return multiplication (big * x).
	 * aux is an auxiliary struc used in computation, and must allocate
	 * same size than "big".
	 */
	if (!bigint_is_zero(big)) {
		if (bigint_is_zero(x)) {
			bigint_set_u32(big, 0);
		} else {
			uint32_t rmbit = bigint_get_right_most_on_bit(x);
			bigint_shift_left(big, rmbit);
	    
			bigint_copy(aux, big);
			int shift_bits = rmbit;
			uint32_t rmword = rmbit >> BXW_2K;
	    
			for (uint32_t b = (rmbit & BXW_MOD_MASK) + 1; b < BITSXWORD; b++) {
				if (x->bits[rmword] & (1 << b)) {
					uint32_t i = (rmword << BXW_2K) + b;
					bigint_shift_left(aux, i - shift_bits);
					shift_bits = i;
					bigint_add_from_word(big, aux, rmword);
				}
			}
			for (uint32_t w = rmword + 1; w < x->len; w++) {
				if (x->bits[w]) {
					for (uint32_t b = 0; b < BITSXWORD; b++) {
						if (x->bits[w] & (1 << b)) {
							uint32_t i = (w << BXW_2K) + b;
							bigint_shift_left(aux, i - shift_bits);
							shift_bits = i;
							bigint_add_from_word(big, aux, w);
						}
					}
				}
			}
		}
	}
}

void bigint_div_u32(bigint_t *big, uint32_t div, uint32_t *res)
{
	if (!div) {
		/* PENDING: Set infty */
		bigint_set_max(big);
		*res = 0;
	} else {
		*res = 0;
		int lmword = big->len - 1;
		for (int i = lmword; i >= 0; i--) {
			uint64_t aux = (uint64_t)(*res) << BITSXWORD;
			*res = (aux + big->bits[i]) % div;
			big->bits[i] = (aux + big->bits[i]) / div;
		}
		bigint_update_len(big);
	}
}

void bigint_div_u64(bigint_t *big, uint64_t div, uint64_t *res)
{
	if (!div) {
		/* PENDING: Set infty */
		bigint_set_max(big);
		*res = 0;
	} else {
		int cmp = bigint_compare_u64(big, div);
		if (cmp == 0) {
			bigint_set_u32(big, 1);
			*res = 0;
		} else if (cmp < 0) {
			*res = bigint_truncate_u64(big);
			bigint_set_u32(big, 0);
		} else {
			if (big->len < 2) {
				uint64_t N = (uint64_t) big->bits[1];
				N <<= BITSXWORD;
				N |= (uint64_t) big->bits[0];
				*res = N % div;
				bigint_set_u64(big, N / div);
			} else {
				*res = 0;
				int lmword = big->len;
				for (int b = BITSXWORD - 1; b >= 0; b--) {
					uint32_t i = (lmword << BXW_2K) + b;
					uint32_t mask = (1 << b);
					(*res) <<= 1;
					if (big->bits[lmword] & mask)
						(*res) |= 1;
		    
					if (*res >= div) {
						(*res) -= div;
						big->bits[lmword] |= mask; 
					} else {
						big->bits[lmword] &= ~mask; 
					}
				}
				for (int w = lmword - 1; w >= 0; w--) {
					if (big->bits[w] || *res) {
						for (int b = BITSXWORD - 1; b >= 0; b--) {
							uint32_t i = (w << BXW_2K) + b;
							uint32_t mask = (1 << b);
							(*res) <<= 1;
							if (big->bits[w] & mask)
								(*res) |= 1;
		    
							if (*res >= div) {
								(*res) -= div;
								big->bits[w] |= mask; 
							} else {
								big->bits[w] &= ~mask; 
							}
						}
					}
				}
				bigint_update_len(big);
			}
		}
	}
}

void bigint_div_2k(bigint_t *big, uint32_t k)
{
	bigint_shift_right(big, k);
}

void bigint_div(bigint_t *big, const bigint_t *div, bigint_t *res)
{
	if (bigint_is_zero(div)) {
		/* PENDING: Set infty */
		bigint_set_max(big);
		bigint_set_u32(res, 0);
	} else {
		int cmp = bigint_compare(big, div);
		if (cmp == 0) {
			bigint_set_u32(big, 1);
			bigint_set_u32(res, 0);
		} else if (cmp < 0) {
			bigint_copy(res, big);
			bigint_set_u32(big, 0);
		} else {
			if (big->len < 2) {
				bigint_set_u32(res, big->bits[0] % div->bits[0]);
				bigint_set_u32(big, big->bits[0] / div->bits[0]);
			} else {
				bigint_set_u32(res, 0);
				int lmword = big->len;
				for (int b = BITSXWORD - 1; b >= 0; b--) {
					uint32_t i = (lmword << BXW_2K) + b;
					uint32_t mask = (1 << b);
					bigint_shift_left(res, 1);
					if (big->bits[lmword] & mask)
						bigint_set_lsbit(res);
		    
					if (bigint_compare(res, div) >= 0) {
						bigint_subtract(res, div);
						big->bits[lmword] |= mask; 
					} else {
						big->bits[lmword] &= ~mask; 
					}
				}
				for (int w = lmword - 1; w >= 0; w--) {
					if (big->bits[w] || bigint_gt(res, 0)) {
						for (int b = BITSXWORD - 1; b >= 0; b--) {
							uint32_t i = (w << BXW_2K) + b;
							uint32_t mask = (1 << b);
							bigint_shift_left(res, 1);
							if (big->bits[w] & mask)
								bigint_set_lsbit(res);
		    
							if (bigint_compare(res, div) >= 0) {
								bigint_subtract(res, div);
								big->bits[w] |= mask; 
							} else {
								big->bits[w] &= ~mask; 
							}
						}
					}
				}
				bigint_update_len(big);
			}
		}
	}
}

void bigint_div_2kless1(bigint_t *big, uint32_t k, bigint_t *res)
{
	bigint_copy(res, big);
	bigint_set_u32(big, 0);
	
	while (bigint_compare_2k(res, k) >= 0) {
		add_N_div2k(big, res, k);
		set_div2k_plus_res2k(res, k);
	}

	if (bigint_compare_2kless1(res, k) >= 0) {
		bigint_add_u32(big, 1);
		bigint_add_u32(res, 1);
		bigint_subtract_2k(res, k);
	}
}

static void add_N_div2k(bigint_t *big, const bigint_t *add, uint32_t k)
{
	/* Return big + adding / 2k */
	uint32_t iword = k >> BXW_2K;
	if (iword >= add->len)
		return;
	
	uint32_t ibit = k & BXW_MOD_MASK;
        
	uint32_t len = MAX(big->len, add->len - iword);
	if (len > big->words)
		bigint_duplicate_words(big, len);

	uint32_t i;
	uint64_t sum = 0;
	for (i = 0; i < len; i++) {
		uint32_t j = i + iword;
		if (j < add->len) {
			uint32_t adding = (add->bits[j] >> ibit);
			if (j + 1 < add->len) {
				int N = BITSXWORD - ibit;
				adding |= (add->bits[j + 1] << N);
			}
			sum += (uint64_t) big->bits[i] + adding;
		} else {
			if (!sum)
				break;
			sum += (uint64_t) big->bits[i];
		}
		big->bits[i] = (uint32_t) sum;
		sum >>= BITSXWORD;
	}
	if (sum) {
		big->bits[len] = sum;
		len ++;
	}
	big->len = len;
}

static void set_div2k_plus_res2k(bigint_t *big, uint32_t k)
{
	/* Return big / 2k + big % 2k
	 * Example:
	 *  Big: 101101101111
	 *            ^
	 *    k: 6
	 *  Big / k:  101101
	 *  Big % k:  101111
	 *  Result : 1011100
	 */
	uint32_t iword = k >> BXW_2K;
	if (iword >= big->len)
		return;
	
	uint32_t ibit = k & BXW_MOD_MASK;
	int cbit = BITSXWORD - ibit;
	
	uint32_t len = MAX(iword, big->len - iword);

	uint32_t i;
	uint64_t sum = 0;
	for (i = 0; i < len; i++) {
		uint32_t j = i + iword;
		if (j < big->len) {
			uint32_t adding = big->bits[j] >> ibit;
			if (j + 1 < big->len) {
				adding |= big->bits[j + 1] << cbit;
			}
			if (j == iword)
				big->bits[j] = (big->bits[j] << cbit) >> cbit;
			else
				big->bits[j] = 0;
			sum += (uint64_t) big->bits[i] + adding;
		} else {
			if (!sum)
				break;
			sum += (uint64_t) big->bits[i];
		}
		big->bits[i] = (uint32_t) sum;
		sum >>= BITSXWORD;
	}
	if (big->bits[len-1]) {
		big->len = len;
	} else {
		big->len = len - 1;
	}
}

void bigint_div_fast(bigint_t *big, const bigint_t *div, bigint_t *res,
		     bigint_t *aux1, bigint_t *aux2, bigint_t *aux3)
{
	/* Perform 10x better than bigint_div if quotient and divisor have 
	 * similar magnitude.
	 *  > aux1 (delta) and aux2 (quotient) are used in computation
	 */
	bigint_copy(res, big);
	bigint_set_u32(big, 0);
	bigint_set_u32(aux1, 0);
	uint32_t n = bigint_get_2k_geq(div);
	bigint_add_2k(aux1, n);
	bigint_subtract(aux1, div);

	while (bigint_compare_2k(res, n) > 0 &&
	       bigint_compare(res, div) > 0) {
		bigint_copy(aux2, res);
		bigint_div_2k(aux2, n);
		bigint_add(big, aux2);

		bigint_mul(aux2, aux1, aux3);
		
		bigint_mod_2k(res, n);
		bigint_add(res, aux2);
	}

	if (bigint_compare(res, div) >= 0) {
		bigint_add_u32(big, 1);
		bigint_subtract(res, div);
	}
}

void bigint_mod_2k(bigint_t *big, uint32_t k)
{
	uint32_t mod = k & 31U;
	uint32_t iw = (k >> BXW_2K) - ((mod > 0) ? 0 : 1);
	uint32_t mask = (1U << mod) - 1;
	
	uint32_t word;
	bigint_get_word(big, iw, &word);

	uint32_t i = iw + 1;
	while (i < big->len) {
		big->bits[i] = 0;
		i ++;
	}
	big->len = iw + 1;
	
	bigint_set_word(big, iw, word & mask);
}

void bigint_mod_2kless1(bigint_t *big, uint32_t k)
{
	while (bigint_compare_2k(big, k) >= 0) {
		set_div2k_plus_res2k(big, k);
	}

	if (bigint_compare_2kless1(big, k) >= 0) {
		bigint_add_u32(big, 1);
		bigint_subtract_2k(big, k);
	}
}

void bigint_mod(bigint_t *big, const bigint_t *div,
		bigint_t *aux1, bigint_t *aux2, bigint_t *aux3)
{
	/* Auxiliary structures used in computation:
	 *   > aux1 (delta)
	 *   > aux2 (iterative quotient)
	 */
	bigint_set_u32(aux1, 0);
	uint32_t n = bigint_get_2k_geq(div);
	bigint_set_u32(aux1, 0);
	bigint_add_2k(aux1, n);
	bigint_subtract(aux1, div);

	while (bigint_compare_2k(big, n) > 0 &&
	       bigint_compare(big, div) > 0) {
		bigint_copy(aux2, big);
		bigint_div_2k(aux2, n);

		bigint_mul(aux2, aux1, aux3);
		
		bigint_mod_2k(big, n);
		bigint_add(big, aux2);
	}

	if (bigint_compare(big, div) >= 0) {
		bigint_subtract(big, div);
	}
}

void bigint_get_binary_string(const bigint_t *big, char *str)
{
	uint32_t i, j;
	uint32_t len = big->len;
	for (i = 0; i < len; i++) {
		uint32_t a = big->bits[i];
		for (j = 0; j < BITSXWORD; j++) {
			uint32_t idx = ((len - i) << BXW_2K) - j - 1;
			if (a & 1)
				str[idx] = '1';
			else
				str[idx] = '0';
			a >>= 1;
		}
	}
	str[len << BXW_2K] = '\0';
}

void bigint_get_hexadec_string(const bigint_t *big, char *str)
{
	int len = big->len;
	if (len) {
		int j = 0;
		uint32_t w = big->bits[len - 1];
		for (int k = 0; k < 8; k++) {
			int bits = 4 * (7 - k);
			int v = (w >> bits) & 15;
			if (v || j) {
				str[j] = char_dec2hex(v);
				j ++;
			}
		}
		for (int i = 1; i < len; i++) {
			w = big->bits[len - 1 - i];
			for (int k = 0; k < 8; k++) {
				int bits = 4 * (7 - k);
				int v = (w >> bits) & 15;
				if (v || j) {
					str[j] = char_dec2hex(v);
					j ++;
				}
			}
		}
		str[j] = 0;
	} else {
		strcpy(str, "0");
	}
}

static char char_dec2hex(int n)
{
	if (n < 10)
		return (char) (n + 48);
	else if (n < 16)
		return (char) (n - 10 + 65);
	else
		return '?'; // Report error
}

static void reverse_string(char *str, int len)
{
	uint32_t n = len / 2;
	for (uint32_t i = 0; i < n; i++) {
		char aux = str[i];
		str[i] = str[len - 1 - i];
		str[len - 1 - i] = aux;
	}

}

void bigint_get_decimal_string(const bigint_t *big, char* str)
{
	if (big->len) {
		bigint_t *cp = bigint_clone(big);
		uint32_t i = 0;
		for (int d = 8; d > 0; d--) {
			uint32_t m = 10;
			for (int j = 1; j < d; j++)
				m *= 10;
	    
			while (bigint_gt(cp, 0)) {
				uint32_t res;
				bigint_div_u32(cp, m, &res);
				uint32_t zeros = 0;
				uint32_t mcp = m / 10;
				int gtz = bigint_gt(cp, 0);
				while (gtz && mcp > res) {
					mcp /= 10;
					zeros ++;
				}
				while (res) {
					int r = res % 10;
					res /= 10;
					str[i] = r + 48;
					i ++;
				}	
				while (gtz && zeros) {
					zeros --;
					str[i] = '0';
					i ++;
				}
			}
		}
		bigint_destroy(cp);
		str[i] = '\0';
		reverse_string(str, i);
	} else {
		strcpy(str, "0");
	}
}

void bigint_pow2(bigint_t *big, bigint_t *aux)
{
	bigint_copy(aux, big);
	uint32_t rmbit = bigint_get_right_most_on_bit(aux);
	bigint_shift_left(big, rmbit);

	uint32_t rmword = rmbit >> BXW_2K;
	    
	for (uint32_t b = (rmbit & BXW_MOD_MASK) + 1; b < BITSXWORD; b++) {
		if (aux->bits[rmword] & (1 << b)) {
			uint32_t i = (rmword << BXW_2K) + b;
			bigint_shift_left(aux, i);
			bigint_add_from_word(big, aux, rmword);
			bigint_shift_right(aux, i);
		}
	}
	for (uint32_t w = rmword + 1; w < aux->len; w++) {
		if (aux->bits[w]) {
			for (uint32_t b = 0; b < BITSXWORD; b++) {
				if (aux->bits[w] & (1 << b)) {
					uint32_t i = (w << BXW_2K) + b;
					bigint_shift_left(aux, i);
					bigint_add_from_word(big, aux, w);
					bigint_shift_right(aux, i);
				}
			}
		}
	}
}

void bigint_pow(bigint_t *big, uint32_t p, bigint_t *aux)
{
	if (p == 0) {
		bigint_set_u32(big, 1);
	} else if (p == 2) {
		bigint_pow2(big, aux);
	} else if (p > 2) {
		bigint_t *q = bigint_clone(big);
		for (int i = 1; i < p; i++)
			bigint_mul(big, q, aux);
		bigint_destroy(q);
	}
}

static uint32_t sqrt_u32(uint32_t n, uint32_t *res)
{
	*res  = n;
	n = 0;
	uint32_t bit = 1;
	bit <<= 30;

	while (bit > *res)
		bit >>= 2;

	while (bit) {
		if (*res >= n + bit) {
			(*res) -= (n + bit);
			n += (bit << 1);
		}
		n >>= 1;
		bit >>= 2;
	}
	return n;
}

static uint64_t sqrt_u64(uint64_t n, uint64_t *res)
{
	*res  = n;
	n = 0;
	uint64_t bit = 1;
	bit <<= 62;

	while (bit > *res)
		bit >>= 2;

	while (bit) {
		if (*res >= n + bit) {
			(*res) -= (n + bit);
			n += (bit << 1);
		}
		n >>= 1;
		bit >>= 2;
	}
	return n;
}

static uint32_t get_2k_4div_leq(const bigint_t *big)
{
	uint32_t w = 1;
	w <<= 30;
	uint32_t word = big->bits[big->len - 1];
	uint32_t bit = 0;
	while (w > word) {
		w >>= 1;
		bit ++;
	}
	return bit + (big->len - 1) << BXW_2K;
}

static void sqrt_ubig(bigint_t *big, bigint_t *res)
{
	bigint_copy(res, big);
	bigint_set_u32(big, 0);
	
	uint32_t bit = get_2k_4div_leq(res);
	
	while (1) {
		bigint_add_2k(big, bit);
		if (bigint_compare(res, big) >= 0) {
			bigint_subtract(res, big);
			bigint_add_2k(big, bit);
		} else {
			bigint_subtract_2k(big, bit);
		}
		bigint_shift_right(big, 1);
		if (bit < 2)
			break;
		bit -= 2;
	}
}

void bigint_sqrt(bigint_t *big, bigint_t *res)
{
	if (big->len < 2) {		
		uint32_t a = bigint_truncate_u32(big);
		uint32_t r;
		a = sqrt_u32(a, &r);
		bigint_set_u32(big, a);
		bigint_set_u32(res, r);
	} else if (big->len == 2) {
		uint64_t a = bigint_truncate_u64(big);
		uint64_t r;
		a = sqrt_u64(a, &r);
		bigint_set_u64(big, a);
		bigint_set_u64(res, r);
	} else {
		sqrt_ubig(big, res);
	}
}
