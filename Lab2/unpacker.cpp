#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <string>
using namespace std;

typedef struct {
    uint64_t magic;     /* 'BINFLAG\x00' */
    uint32_t datasize;  /* in big-endian */
    uint16_t n_blocks;  /* in big-endian */
    uint16_t zeros;
} __attribute((packed)) binflag_header_t;

typedef struct {
    uint32_t offset;        /* in big-endian */
    uint16_t cksum;         /* XOR'ed results of each 2-byte unit in payload */
    uint16_t length;        /* ranges from 1KB - 3KB, in big-endian */
    uint8_t  payload[0];
} __attribute((packed)) block_t;

typedef struct {
   uint16_t length;        /* length of the offset array, in big-endian */
   uint32_t offset[0];     /* offset of the flags, in big-endian */
} __attribute((packed)) flag_t;

uint16_t convert_indian_u16(uint16_t indianValue) {
    uint16_t result = 0;
    result |= (indianValue & 0xFF00) >> 8;
    result |= (indianValue & 0x00FF) << 8;
    return result;
}

uint32_t convert_indian_u32(uint32_t indianValue) {
    uint32_t result = 0;
    result |= (indianValue & 0xFF000000) >> 24;
    result |= (indianValue & 0x00FF0000) >> 8;
    result |= (indianValue & 0x0000FF00) << 8;
    result |= (indianValue & 0x000000FF) << 24;
    return result;
}

char int2hexstr(u_int8_t a) {
    return char(int(a)>=10?int(a)-10+'a':int(a)+'0');
}

int main(int argc, char* argv[]){
    printf(" ");
    int srcfd = open("./demo.bin", O_RDONLY);
    binflag_header_t *hdr;
    int b_read = read(srcfd, hdr, sizeof(binflag_header_t));
    hdr->n_blocks = convert_indian_u16(hdr->n_blocks);
    hdr->datasize = convert_indian_u32(hdr->datasize);
    block_t *blocks = (block_t*)malloc(hdr->n_blocks * sizeof(block_t));
    uint8_t *data = (uint8_t*)malloc(8000000 * sizeof(uint8_t));
    vector<uint8_t> v;
    for (u_int16_t i=0;i<hdr->n_blocks;i++){
        
        b_read = read(srcfd, &blocks[i], sizeof(block_t));
        if (b_read < 0) {
            printf("read error \n");
            return 0;
        }
        
        blocks[i].offset = convert_indian_u32(blocks[i].offset);
        blocks[i].length = convert_indian_u16(blocks[i].length);
        blocks[i].cksum = convert_indian_u16(blocks[i].cksum);
        b_read = read(srcfd, blocks[i].payload, blocks[i].length * sizeof(u_int8_t));
        if (b_read < 0) {
            printf("%d read error \n", i);
            return 0;
        }
        uint16_t check_chsum = 0;
        for (int j = 0; j < blocks[i].length; j += 2) {
            check_chsum ^= ((blocks[i].payload[j] << 8) + (blocks[i].payload[j + 1]));
        }
        if(check_chsum == blocks[i].cksum){
            for(u_int16_t k=0;k<blocks[i].length;k++){
                int id = int(blocks[i].offset)+int(k);
                data[id*2] = (blocks[i].payload[k]>>4);
                data[id*2+1] = (blocks[i].payload[k] & (0x0F));
            }
        }
        
    }

    flag_t *flag;
	b_read = read(srcfd, flag, sizeof(flag_t));
    if (b_read < 0) {
        printf("read error \n");
        return 0;
    }
    flag->length = convert_indian_u16(flag->length);
    //const vector<uint8_t> tmp = data;
    
    b_read = read(srcfd, flag->offset, flag->length * sizeof(u_int32_t));
    if (b_read < 0) {
        printf("read error \n");
        return 0;
    }
    close(srcfd);
	for(int i=0;i<flag->length;i++) {
        int id = convert_indian_u32(flag->offset[i])*2;
        for(int j=0;j<4;j++){
            printf("%c", int2hexstr(data[id+j]));
        }
    }
    
    return 0;
}
