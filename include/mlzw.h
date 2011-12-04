#ifndef MLZW_H
#define MLZW_H
#include <stdint.h>

typedef struct lzw_element_st
{
    uint32_t next[256];
    uint32_t prev;
    uint64_t * hits;
    int size;
    uint32_t c;
} lzw_t;
typedef struct lzw_handle_t
{
    lzw_t * dictionary;
    lzw_t * reverse_dictionary;
    size_t bytes;
} mlzw_handle;
typedef struct mlzw_encoding_st
{
    char * data;
    int size;
} mlzw_encoding;

lzw_t * mlzw_make_dict(mlzw_handle * h, void * bytes, int size);
int mlzw_save_handle(mlzw_handle *h, char * filename);
mlzw_encoding * mlzw_encode(mlzw_handle *h,void * bytes, int size);
mlzw_encoding * mlzw_decode(mlzw_handle*h,void * bytes,int size);
mlzw_handle * mlzw_new();
#endif
