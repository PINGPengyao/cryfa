/*
 * This file is part of quip.
 *
 * Copyright (c) 2012 by Daniel C. Jones <dcjones@cs.washington.edu>
 *
 */


/*
 * qualenc:
 * Statistical modeling of sequence quality scores.
 */

#ifndef QUIP_QUALENC
#define QUIP_QUALENC

#include "quip.h"
#include <stdint.h>


typedef struct qualenc_t_ qualenc_t;

qualenc_t* qualenc_alloc_encoder(quip_writer_t writer, void* writer_data);
qualenc_t* qualenc_alloc_decoder(quip_reader_t reader, void* reader_data);
void       qualenc_free(qualenc_t*);

void   qualenc_set_base_qual(qualenc_t*, char);

void   qualenc_encode(qualenc_t*, const short_read_t*);
size_t qualenc_finish(qualenc_t*);
void   qualenc_flush(qualenc_t*);

void qualenc_decode(qualenc_t*, short_read_t*, size_t n);
void qualenc_start_decoder(qualenc_t*);
void qualenc_reset_decoder(qualenc_t*);

#endif

