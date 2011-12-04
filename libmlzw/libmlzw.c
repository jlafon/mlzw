#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "mlzw.h"
#define MAX_DICT_SIZE 65536


mlzw_handle * mlzw_new()
{
    mlzw_handle * h = calloc(1,sizeof(*h));
    h->dictionary = NULL;
    return h;
}  
int mlzw_load_handle(mlzw_handle *h, char*filename)
{
    int infd = open(filename, O_RDONLY);
    if(infd < 0)
    {
        perror("Unable to open input file");
        return errno;
    }
    int size = 0;
    int status = read(infd,&size,sizeof(size));
    if(status < 0)
    {
        perror("Unable to read from input file");
        return errno;
    }
    h->dictionary = calloc(size,sizeof(*h->dictionary));
    h->reverse_dictionary = calloc(size,sizeof(*h->reverse_dictionary));
    h->dictionary->size = h->reverse_dictionary->size = size;
    int i;
    for(i = 0; i < h->dictionary->size; i++)
        if(read(infd,&h->dictionary[i],sizeof(h->dictionary[i])) < 0)
        {
            perror("Error while reading");
            return errno;
        }
    for(i = 0; i < h->dictionary->size; i++)
    {
        if(read(infd,&h->reverse_dictionary[i],sizeof(h->reverse_dictionary[i])) < 0)
        {
            perror("Error while reading");
            return errno;
        }
    }
    return close(infd);
}
int mlzw_save_handle(mlzw_handle *h, char*filename)
{
    int outfd = open(filename, O_CREAT | O_TRUNC | O_WRONLY,0644);
    if(outfd < 0)
    {
        perror("Unable to open output file");
        return errno;
    }
    int status = write(outfd,&h->dictionary->size,sizeof(h->dictionary->size));
    if(status < 0)
    {
        perror("Unable to write to output file");
        return errno;
    }
    int i;
    for(i = 0; i < h->dictionary->size; i++)
        if(write(outfd,&h->dictionary[i],sizeof(h->dictionary[i])) < 0)
        {
            perror("Error while writing");
            return errno;
        }
    for(i = 0; i < h->dictionary->size; i++)
    {
        if(write(outfd,&h->reverse_dictionary[i],sizeof(h->reverse_dictionary[i])) < 0)
        {
            perror("Error while writing");
            return errno;
        }
    }
    return close(outfd);;
}
lzw_t * mlzw_make_dict(mlzw_handle * h,void * bytesource, int size)
{
    lzw_t *dictionary = calloc(4096,sizeof(*dictionary));
    lzw_t *decode_dict = calloc(4096,sizeof(*decode_dict));
    uint8_t * source = (uint8_t *)bytesource;
    uint32_t c,code,nc;
    uint32_t dict_size = 256;
    int d_size = 4096;
    for(code = *(source++); size--;)
    {
        c = *(source++);
        if((dictionary[code].next[c]))
        {
            nc = dictionary[code].next[c];
            code = nc;
        }
        else
        {
            if(dict_size+1 < MAX_DICT_SIZE)
            {
                if(dict_size == d_size-1)
                {
                    dictionary = realloc(dictionary,sizeof(*dictionary)*(d_size+4096));
                    decode_dict = realloc(decode_dict,sizeof(*decode_dict)*(d_size+4096));
                    d_size = d_size+4096;
                }
                decode_dict[dict_size].prev = code;
                decode_dict[dict_size].c = c;
                nc = dictionary[code].next[c] = dict_size++;
            }
            code = c;
        }
    }
    dictionary->size = dict_size;
    h->dictionary = dictionary;
    h->reverse_dictionary = decode_dict;
    return dictionary;
}
mlzw_encoding * mlzw_encode(mlzw_handle * h, void * bytes, int size)
{
    printf("Input: %d bytes\n",size);
    lzw_t *dictionary = h->dictionary;
    size_t output_size = 4096;
    char* output = calloc(1,output_size*sizeof(*output));
    size_t written = 0;
    uint32_t c,code,nc;
    off_t index = 0;
//    uint32_t dict_size = h->dictionary->size;
    uint8_t * source = (uint8_t*)bytes;
    for(code = *(source++); size--;)
    {
        c = *(source++);
        if((dictionary[code].next[c]))
        {
            nc = dictionary[code].next[c];
            code = nc;
        }
        else
        {
            if(written == output_size)
            {
                output_size+=4096;
                output = realloc(output,output_size*sizeof(*output));
                
            }
            memcpy((output+index),&code,sizeof(code));
            index+=4;
            written += sizeof(code);
            code = c;
        }
    }
    printf("Compressed to: %zd bytes\n",written);
    mlzw_encoding * result = calloc(1,sizeof(*result));
    result->data = output;
    result->size = written;
    return result;
}

mlzw_encoding * mlzw_decode(mlzw_handle*h,void * bytes,int size)
{
    uint32_t * input = (uint32_t*)bytes;
    lzw_t *dictionary = h->dictionary;
    lzw_t *decode_dict = h->reverse_dictionary;
    mlzw_encoding * result = calloc(1,sizeof(*result));
    uint32_t c;
    uint32_t * end = input + (size/sizeof(uint32_t));
    size_t output_size = 4096;
    char * output = calloc(1,sizeof(*output)*output_size);
    off_t index = 0;
    uint32_t depth = 0;
    for(;input< end;)
    {
        c = *input++;
        if(c <= 255)
        {
            if(index == output_size)
            {
                output_size+=4096;
                output = realloc(output,output_size*sizeof(*output));
            }
            memcpy((output+index),&c,1);
            index++;
        }
        else
        {
            depth = 0;
            dictionary[depth++].prev = decode_dict[c].c; 
            c = decode_dict[c].prev;
            while(c > 255)
            {
                dictionary[depth++].prev = decode_dict[c].c; 
                c = decode_dict[c].prev;
            }
            if((index+depth) == output_size)
            {
                output_size+=(depth + 4096);
                output = realloc(output,output_size*sizeof(*output));
            }
            memcpy((output+index),&c,1);
            index++;
            while(depth--)
            {
                memcpy((output+index),&dictionary[depth].prev,1);
                index++;
            }
        }
    }
   // printf("Decompressed to: %zd bytes\n",index);
    result->data = output;
    result->size = index;
    return result;
}
