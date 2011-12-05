#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "../include/mlzw.h"

/* Note that these values must keep the relationship 
 * 2^16 = 65536
 */
#define MAX_DICT_SIZE 65536
#define COMPRESSION_BITS 16

/* Use a maximum of 64 bits to encode a symbol */
#define MAX_TREE_DEPTH 64

#define bin_pattern "%u%u%u%u%u%u%u%u %u%u%u%u%u%u%u%u %u%u%u%u%u%u%u%u %u%u%u%u%u%u%u%u"
#define binary(pattern) \
    (pattern &0x80000000 ? 1 : 0), \
    (pattern &0x40000000 ? 1 : 0), \
    (pattern &0x20000000 ? 1 : 0), \
    (pattern &0x10000000 ? 1 : 0), \
    (pattern &0x8000000 ? 1 : 0), \
    (pattern &0x4000000 ? 1 : 0), \
    (pattern &0x2000000 ? 1 : 0), \
    (pattern &0x1000000 ? 1 : 0), \
    (pattern &0x800000 ? 1 : 0), \
    (pattern &0x400000 ? 1 : 0), \
    (pattern &0x200000 ? 1 : 0), \
    (pattern &0x100000 ? 1 : 0), \
    (pattern &0x80000 ? 1 : 0), \
    (pattern &0x40000 ? 1 : 0), \
    (pattern &0x20000 ? 1 : 0), \
    (pattern &0x10000 ? 1 : 0), \
    (pattern &0x8000 ? 1 : 0), \
    (pattern &0x4000 ? 1 : 0), \
    (pattern &0x2000 ? 1 : 0), \
    (pattern &0x1000 ? 1 : 0), \
    (pattern &0x800 ? 1 : 0), \
    (pattern &0x400 ? 1 : 0), \
    (pattern &0x200 ? 1 : 0), \
    (pattern &0x100 ? 1 : 0), \
    (pattern &0x80 ? 1 : 0), \
    (pattern &0x40 ? 1 : 0), \
    (pattern &0x20 ? 1 : 0), \
    (pattern &0x10 ? 1 : 0), \
    (pattern &0x08 ? 1 : 0), \
    (pattern &0x04 ? 1 : 0), \
    (pattern &0x02 ? 1 : 0), \
    (pattern &0x01 ? 1 : 0)

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
void
mlzw_create_sampling(mlzw_handle *h, void * bytes, int size)
{
    lzw_t *dictionary = h->dictionary;
    lzw_t *decode_dict = h->reverse_dictionary;
    uint32_t c,code,nc;
    off_t index = 0;
    uint64_t total = 0;
    uint32_t depth = 0;
    int i = 0;
    uint64_t * frequency = calloc(1,sizeof(*frequency)*h->dictionary->size);
    uint32_t *huffman_dictionary = calloc(h->dictionary->size,sizeof(*huffman_dictionary));
    uint8_t *bits = calloc(h->dictionary->size,sizeof(*bits));
    mlzw_sorted_list * new;
    mlzw_sorted_list * head = NULL;
    mlzw_sorted_list * temp = NULL;
    int new_value = 0;
    typedef struct lzw_coin_t
    {
        float denom;
        int hits;
    } lzw_coin;
    typedef struct symbol_t
    {
        int code;
        lzw_coin * coins; 
    } symbol;
    inline void print_list()
    {
        printf("List: \n");
        temp = head;
        while((temp = temp->next))
        {
            printf("ID: %10d Hits: %10u Prob: %0.10f\n",temp->value, temp->hits,(double)temp->hits/(double)total);
        }
    }
    inline void list_push()
    {
        if(!head)
        {
            head = calloc(1,sizeof(*head));
            head->value = new->value;
            head->hits = new->hits;
            head->right = new->right;
            head->left = new->left;
            head->weight = new->weight;
            free(new);
        }
        else
        {
            temp = head;
            while(temp->next && temp->next->hits < new->hits)
                temp = temp->next;
            if(temp == head)
            {
                new->next = head;
                head->prev = new;
                head = new;
            }
            else
            {
                if(temp->next)
                    temp->next->prev = new;
                new->next = temp->next;
                temp->next = new;
                new->prev = temp;
            }
        }

    }
    inline void print_patterns(mlzw_sorted_list * root, uint32_t pattern, int depth)
    {
        if(!root)
            return;
        if(!root->left && !root->right)
        {
            printf("ID: %12d Weight: %u Prob: %0.10f Pattern: "bin_pattern"\n",root->value,root->weight,(double)root->hits/(double)total,binary(pattern));
            huffman_dictionary[root->value] = pattern;
            bits[root->value] = depth;
        }
        else
        {
            print_patterns(root->right,(pattern << 1)|1,depth+1);
            printf("weight = %u\n",root->weight);
            print_patterns(root->left,(pattern << 1),depth+1);
        }
        
    }
    inline void padding(int i)
    {
        int j;
        for(j=0; j < i; j++)
            printf("\t");
    }
    inline void print_tree(mlzw_sorted_list * root, int depth)
    {
        if(!root)
            return;
        print_tree(root->right,depth+1);
        padding(depth);
        printf("%d\n",root->value);
        print_tree(root->left,depth+1);
    }
    inline void package_merge()
    {
        symbol * symbols = calloc(h->dictionary->size,sizeof(*symbols));
        int q,p;
        temp = head;
        /* Create a list of symbols, based on frequencies */
        for(q=0; q< h->dictionary->size; q++)
        {
            symbols[q].coins = calloc(COMPRESSION_BITS,sizeof(lzw_coin));
            for(p=0; p < COMPRESSION_BITS; p++)
            {
                symbols[q].coins[p].denom = p;
                symbols[q].coins[p].hits = head->hits;
            }
            symbols[q].code = head->value;
        }
        for(q=0; q< COMPRESSION_BITS; q++)
        {

        }

    }
    inline void generate_tree()
    {
        while(head && head->next)
        {
            new = calloc(1,sizeof(*head));
            new->hits = head->hits + head->next->hits;
            new->left = head;
            new->right = head->next;
            new->value = 0;
            new->weight = 1;
            if(head->next->weight > new->weight)
                new->weight += head->next->weight;
            else
                new->weight += head->weight;
            
            head = head->next->next;
            list_push();
        }
        //print_tree(head,0);
    }
    uint8_t * source = (uint8_t*)bytes;
    for(code = *(source++); size--;)
    {
        c = *(source++);
        if((dictionary[code].next[c]))
        {
            frequency[dictionary[code].next[c]]++;
            nc = dictionary[code].next[c];
            code = nc;
            total++;
        }
        else
        {
            code = c;
        }
    }
    for(i=0; i<h->dictionary->size; i++)
    {
        if(frequency[i] > 0 || i < 256)
        {
            new = calloc(1,sizeof(*new));
            new->value = i;
            new->hits = frequency[i];
            list_push();
        }
    }
    print_list();
    generate_tree();
    //print_tree(head,0);
    print_patterns(head, 0,1);
    h->bits = bits;
    h->huffman_dict = huffman_dictionary;
//    h->tree = head;
    return;

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
mlzw_encoding * mlzw_huffman_encode(mlzw_handle * h, void * bytes, int size)
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
            printf("Value: %d Code: %x Bits: %u\n",code,h->huffman_dict[code],h->bits[code]);
            memcpy((output+index),&code,sizeof(code));
            index+=4;
            written += sizeof(code);
            code = c;
        }
    }
    printf("Huffman Compressed to: %zd bytes\n",written);
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
            if((index+depth) >= output_size)
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
