/*
 * This file is part of quip.
 *
 * Copyright (c) 2012 by Daniel C. Jones <dcjones@cs.washington.edu>
 *
 */


/*
 * idenc:
 * Compression and decompression of sequence ids.
 */

#ifndef QUIP_IDENC
#define QUIP_IDENC

#include "quip.h"
#include <stdint.h>


typedef struct idenc_t_ idenc_t;

idenc_t* idenc_alloc_encoder(quip_writer_t writer, void* writer_data);
void     idenc_free(idenc_t*);

void idenc_encode(idenc_t*, const str_t*);
size_t idenc_finish(idenc_t*);
void   idenc_flush(idenc_t*);

idenc_t* idenc_alloc_decoder(quip_reader_t reader, void* reader_data);
void     idenc_decode(idenc_t*, str_t*);

void     idenc_start_decoder(idenc_t*);
void     idenc_reset_decoder(idenc_t*);


#endif

