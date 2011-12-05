#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "../include/mlzw.h"


static inline long long unsigned
time_ns(struct timespec *const ts)
{
    if (clock_gettime (CLOCK_REALTIME, ts))
    {
        exit (1);
    }
    return ((long long unsigned) ts->tv_sec) * 1000000000LLU
    + (long long unsigned) ts->tv_nsec;
}

int main(int argc, char **argv)
{
    mlzw_handle * handle = mlzw_new();
    struct stat st;
    int fd = open(argv[1],O_RDONLY);
    if(fd == -1)
    {
        printf("Unable to open file %s\n",argv[1]);
        return -1;
    }
    fstat(fd,&st);
    char * input = calloc(sizeof(char),st.st_size);
    read(fd,input,st.st_size);
    close(fd);
    mlzw_make_dict(handle,input,st.st_size);
    
   // printf("Dictionary size: %d\n",handle->dictionary->size);
//    char *sample = "<data key=\"name\">rrj203_HCA_1</data>"; 
    struct timespec ts;
    const long long unsigned start_ns = time_ns (&ts);
    mlzw_encoding * compressed = mlzw_encode(handle,input,st.st_size);
    const long long unsigned delta = time_ns (&ts) - start_ns;
    printf("Bandwidth: %f b/s\n",1000000000*(double)st.st_size/((double)delta));
    mlzw_create_sampling(handle,input,st.st_size);
  //  mlzw_encoding * huffman_compressed = mlzw_huffman_encode(handle,input,st.st_size);
//    fd = open("compressed.lzw",O_CREAT | O_TRUNC | O_WRONLY, 0644);
  //  write(fd,compressed->data,compressed->size);
    //close(fd);

//    fd = open("compressed.lzw",O_RDONLY);
  //  fstat(fd,&st);
    //input = calloc(sizeof(char),st.st_size);
 //   read(fd,input,st.st_size);
   // close(fd);
//    mlzw_encoding * decompressed = mlzw_decode(handle,compressed->data,compressed->size);
//    printf("Original text: [%s]\n",sample);
  //  printf("Decoded text: [%s]\n",decompressed->data);
  //    fd = open("decompressed.txt",O_CREAT | O_TRUNC | O_WRONLY, 0644);
    //  write(fd,decompressed->data,decompressed->size);
      //close(fd);

//    mlzw_save_handle(handle,"test.hdl");
  //  mlzw_load_handle(restored,"test.hdl");
    //printf("Dictionary size: %d\n",restored->dictionary->size);
}
