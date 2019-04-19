/*
 *  Test program for MSA instruction SUBS_S.W
 *
 *  Copyright (C) 2019  Wave Computing, Inc.
 *  Copyright (C) 2019  Aleksandar Markovic <amarkovic@wavecomp.com>
 *  Copyright (C) 2019  RT-RK Computer Based Systems LLC
 *  Copyright (C) 2019  Mateja Marjanovic <mateja.marjanovic@rt-rk.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <sys/time.h>
#include <stdint.h>

#include "../../../../include/wrappers_msa.h"
#include "../../../../include/test_inputs_128.h"
#include "../../../../include/test_utils_128.h"

#define TEST_COUNT_TOTAL (                                                \
            (PATTERN_INPUTS_SHORT_COUNT) * (PATTERN_INPUTS_SHORT_COUNT) + \
            (RANDOM_INPUTS_SHORT_COUNT) * (RANDOM_INPUTS_SHORT_COUNT))


int32_t main(void)
{
    char *instruction_name = "SUBS_S.W";
    int32_t ret;
    uint32_t i, j;
    struct timeval start, end;
    double elapsed_time;

    uint64_t b128_result[TEST_COUNT_TOTAL][2];
    uint64_t b128_expect[TEST_COUNT_TOTAL][2] = {
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },    /*   0  */
        { 0xffffffffffffffffULL, 0xffffffffffffffffULL, },
        { 0x5555555555555555ULL, 0x5555555555555555ULL, },
        { 0xaaaaaaaaaaaaaaaaULL, 0xaaaaaaaaaaaaaaaaULL, },
        { 0x3333333333333333ULL, 0x3333333333333333ULL, },
        { 0xccccccccccccccccULL, 0xccccccccccccccccULL, },
        { 0x1c71c71c71c71c71ULL, 0xc71c71c71c71c71cULL, },
        { 0xe38e38e38e38e38eULL, 0x38e38e38e38e38e3ULL, },
        { 0x0000000100000001ULL, 0x0000000100000001ULL, },    /*   8  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x5555555655555556ULL, 0x5555555655555556ULL, },
        { 0xaaaaaaabaaaaaaabULL, 0xaaaaaaabaaaaaaabULL, },
        { 0x3333333433333334ULL, 0x3333333433333334ULL, },
        { 0xcccccccdcccccccdULL, 0xcccccccdcccccccdULL, },
        { 0x1c71c71d71c71c72ULL, 0xc71c71c81c71c71dULL, },
        { 0xe38e38e48e38e38fULL, 0x38e38e39e38e38e4ULL, },
        { 0xaaaaaaabaaaaaaabULL, 0xaaaaaaabaaaaaaabULL, },    /*  16  */
        { 0xaaaaaaaaaaaaaaaaULL, 0xaaaaaaaaaaaaaaaaULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x8000000080000000ULL, 0x8000000080000000ULL, },
        { 0xdddddddedddddddeULL, 0xdddddddedddddddeULL, },
        { 0x8000000080000000ULL, 0x8000000080000000ULL, },
        { 0xc71c71c71c71c71cULL, 0x80000000c71c71c7ULL, },
        { 0x8e38e38e80000000ULL, 0xe38e38e38e38e38eULL, },
        { 0x5555555655555556ULL, 0x5555555655555556ULL, },    /*  24  */
        { 0x5555555555555555ULL, 0x5555555555555555ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x2222222222222222ULL, 0x2222222222222222ULL, },
        { 0x71c71c727fffffffULL, 0x1c71c71d71c71c72ULL, },
        { 0x38e38e39e38e38e4ULL, 0x7fffffff38e38e39ULL, },
        { 0xcccccccdcccccccdULL, 0xcccccccdcccccccdULL, },    /*  32  */
        { 0xccccccccccccccccULL, 0xccccccccccccccccULL, },
        { 0x2222222222222222ULL, 0x2222222222222222ULL, },
        { 0x8000000080000000ULL, 0x8000000080000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x9999999999999999ULL, 0x9999999999999999ULL, },
        { 0xe93e93e93e93e93eULL, 0x93e93e94e93e93e9ULL, },
        { 0xb05b05b080000000ULL, 0x05b05b05b05b05b0ULL, },
        { 0x3333333433333334ULL, 0x3333333433333334ULL, },    /*  40  */
        { 0x3333333333333333ULL, 0x3333333333333333ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0xdddddddedddddddeULL, 0xdddddddedddddddeULL, },
        { 0x6666666766666667ULL, 0x6666666766666667ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x4fa4fa507fffffffULL, 0xfa4fa4fb4fa4fa50ULL, },
        { 0x16c16c17c16c16c2ULL, 0x6c16c16c16c16c17ULL, },
        { 0xe38e38e48e38e38fULL, 0x38e38e39e38e38e4ULL, },    /*  48  */
        { 0xe38e38e38e38e38eULL, 0x38e38e38e38e38e3ULL, },
        { 0x38e38e39e38e38e4ULL, 0x7fffffff38e38e39ULL, },
        { 0x8e38e38e80000000ULL, 0xe38e38e38e38e38eULL, },
        { 0x16c16c17c16c16c2ULL, 0x6c16c16c16c16c17ULL, },
        { 0xb05b05b080000000ULL, 0x05b05b05b05b05b0ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xc71c71c780000000ULL, 0x71c71c71c71c71c7ULL, },
        { 0x1c71c71d71c71c72ULL, 0xc71c71c81c71c71dULL, },    /*  56  */
        { 0x1c71c71c71c71c71ULL, 0xc71c71c71c71c71cULL, },
        { 0x71c71c727fffffffULL, 0x1c71c71d71c71c72ULL, },
        { 0xc71c71c71c71c71cULL, 0x80000000c71c71c7ULL, },
        { 0x4fa4fa507fffffffULL, 0xfa4fa4fb4fa4fa50ULL, },
        { 0xe93e93e93e93e93eULL, 0x93e93e94e93e93e9ULL, },
        { 0x38e38e397fffffffULL, 0x8e38e38f38e38e39ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },    /*  64  */
        { 0x8cace669dace8e38ULL, 0x386f5044e93c5d10ULL, },
        { 0xdc1038226e92c9c0ULL, 0x238e445f53508af8ULL, },
        { 0x80000000ca3072f2ULL, 0x7fffffff5538cd6cULL, },
        { 0x73531997253171c8ULL, 0xc790afbc16c3a2f0ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x4f6351b97fffffffULL, 0xeb1ef41b6a142de8ULL, },
        { 0x8b6eea16ef61e4baULL, 0x7fffffff6bfc705cULL, },
        { 0x23efc7de916d3640ULL, 0xdc71bba1acaf7508ULL, },    /*  72  */
        { 0xb09cae4780000000ULL, 0x14e10be595ebd218ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x8000000080000000ULL, 0x7fffffff01e84274ULL, },
        { 0x7fffffff35cf8d0eULL, 0x80000000aac73294ULL, },
        { 0x749115ea109e1b46ULL, 0x8000000094038fa4ULL, },
        { 0x7fffffff7fffffffULL, 0x80000000fe17bd8cULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
};

    gettimeofday(&start, NULL);

    for (i = 0; i < PATTERN_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < PATTERN_INPUTS_SHORT_COUNT; j++) {
            do_msa_SUBS_S_W(b128_pattern[i], b128_pattern[j],
                           b128_result[PATTERN_INPUTS_SHORT_COUNT * i + j]);
        }
    }

    for (i = 0; i < RANDOM_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < RANDOM_INPUTS_SHORT_COUNT; j++) {
            do_msa_SUBS_S_W(b128_random[i], b128_random[j],
                           b128_result[((PATTERN_INPUTS_SHORT_COUNT) *
                                        (PATTERN_INPUTS_SHORT_COUNT)) +
                                       RANDOM_INPUTS_SHORT_COUNT * i + j]);
        }
    }

    gettimeofday(&end, NULL);

    elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed_time += (end.tv_usec - start.tv_usec) / 1000.0;

    ret = check_results(instruction_name, TEST_COUNT_TOTAL, elapsed_time,
                        &b128_result[0][0], &b128_expect[0][0]);

    return ret;
}
