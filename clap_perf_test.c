/* Standalone benchmark harness for TannerFirl/huffman.
 *
 * Generates a deterministic, representative input buffer (mix of
 * pseudo-random bytes and repeated runs, to give a realistic Huffman
 * symbol distribution), writes it to a temp file, then times
 * huffman_encode_file() (which internally calls do_file_encode(), the
 * function targeted by the optimization commit) using a monotonic clock.
 *
 * This file is intentionally identical between the "before" and "after"
 * trees; only huffman.c differs between the two measurements.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "huffman.h"

#define INPUT_SIZE (30u * 1024u * 1024u) /* 30 MB */

static unsigned long lcg_state = 0;

static unsigned long next_rand(void)
{
	/* Deterministic LCG (glibc-style constants), fixed seed below. */
	lcg_state = (lcg_state * 1103515245UL + 12345UL) & 0x7fffffffUL;
	return lcg_state;
}

static void fill_input(unsigned char* buf, size_t n)
{
	size_t i = 0;
	lcg_state = 424242UL; /* fixed seed for determinism */

	while (i < n)
	{
		/* Alternate between short random runs and repeated-byte runs
		 * to produce a skewed-but-nontrivial symbol distribution,
		 * similar to real-world file contents. */
		unsigned long r = next_rand();
		size_t run_len = 32 + (r % 224); /* 32..255 */
		if (run_len > n - i)
			run_len = n - i;

		if ((r & 1) == 0)
		{
			unsigned char b = (unsigned char)(next_rand() % 256);
			memset(buf + i, b, run_len);
		}
		else
		{
			size_t j;
			for (j = 0; j < run_len; ++j)
				buf[i + j] = (unsigned char)(next_rand() % 256);
		}
		i += run_len;
	}
}

int main(void)
{
	unsigned char* input = malloc(INPUT_SIZE);
	FILE* fin;
	FILE* fout;
	struct timespec t0, t1;
	double elapsed;
	int rc;

	if (!input)
	{
		fprintf(stderr, "malloc failed\n");
		return 1;
	}

	fill_input(input, INPUT_SIZE);

	fin = tmpfile();
	fout = tmpfile();
	if (!fin || !fout)
	{
		fprintf(stderr, "tmpfile failed\n");
		return 1;
	}

	if (fwrite(input, 1, INPUT_SIZE, fin) != INPUT_SIZE)
	{
		fprintf(stderr, "fwrite failed\n");
		return 1;
	}
	rewind(fin);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	rc = huffman_encode_file(fin, fout);
	clock_gettime(CLOCK_MONOTONIC, &t1);

	if (rc != 0)
	{
		fprintf(stderr, "huffman_encode_file failed: %d\n", rc);
		return 1;
	}

	elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

	fclose(fin);
	fclose(fout);
	free(input);

	printf("PERF_TIME_SEC=%.6f\n", elapsed);
	return 0;
}
