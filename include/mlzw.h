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
typedef struct mlzw_sorted_list_t
{
    uint32_t value;
    uint32_t hits;
    struct mlzw_sorted_list_t * next;
    struct mlzw_sorted_list_t * left;
    struct mlzw_sorted_list_t * right;
    uint8_t weight;
} mlzw_sorted_list;

typedef struct lzw_handle_t
{
    lzw_t * dictionary;
    uint32_t * huffman_dict;
    uint8_t * bits;
    lzw_t * reverse_dictionary;
    mlzw_sorted_list * tree;
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
mlzw_encoding * mlzw_huffman_encode(mlzw_handle *h,void * bytes, int size);
mlzw_encoding * mlzw_decode(mlzw_handle*h,void * bytes,int size);
mlzw_handle * mlzw_new();
void  mlzw_create_sampling(mlzw_handle*h,void * bytes, int size);
#endif
