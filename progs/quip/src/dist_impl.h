/*
 * This file is part of quip.
 *
 * Copyright (c) 2012 by Daniel C. Jones <dcjones@cs.washington.edu>
 *
 */

/* WARNING: DO NOT INCLUDE THIS FILE DIRECTLY. You should include "dist.h".
 */


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "ac.h"

typedef struct dfun(t_)
{
    /* number of new observations until the distribution is updated */
    uint16_t update_delay;

    struct {
        uint16_t count;
        uint16_t freq;
    } xs[DISTSIZE];
} dist_t;


/* Allocate a distribution over the alphabet [0, n - 1].
 * The structure is either allocated with the specify intent of either decoding
 * or encoding to save memory.
 * */
void dfun(init) (dist_t*);

/* explicitly set the distribution */
void dfun(set) (dist_t*, const uint16_t* cs);

/* update distribution to reflect calls new observations */
void dfun(update)(dist_t* D);

/* encode a symbol given the distribution and arithmetic coder */
void   dfun(encode)(ac_t*, dist_t*, symb_t);
symb_t dfun(decode)(ac_t*, dist_t*);


/* Conditional probability distribution.
 */

typedef struct cdfun(t_)
{
    /* an array of distributions */
    dist_t* xss;

    /* alphabet over which the distribution is conditioned */
    uint32_t n;

    /* rate at which distributions are updated */
    uint8_t update_rate;
} cond_dist_t;


void cdfun(init) (cond_dist_t*, size_t n);
void cdfun(free) (cond_dist_t*);

void cdfun(set_update_rate) (cond_dist_t*, uint8_t);

void cdfun(setall) (cond_dist_t*, const uint16_t* cs);
void cdfun(setone) (cond_dist_t*, const uint16_t* cs, size_t i);

void cdfun(encode)(ac_t* ac, cond_dist_t* D, uint32_t y, symb_t x);
symb_t cdfun(decode)(ac_t* ac, cond_dist_t* D, uint32_t y);

