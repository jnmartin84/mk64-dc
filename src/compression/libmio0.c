#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <fcntl.h>
#endif

#include "libmio0.h"
#include "utils.h"
// defines

#define MIO0_VERSION "0.1"
#include <kos.h>
#define GET_BIT(buf, bit) ((buf)[(bit) / 8] & (1 << (7 - ((bit) % 8))))

// decode MIO0 header
// returns 1 if valid header, 0 otherwise
int mio0_decode_header(const unsigned char *buf, mio0_header_t *head)
{
//   printf("%c%c%c%c\n", buf[0],buf[1],buf[2],buf[3]);
   if (!memcmp(buf, "MIO0", 4)) {
      head->dest_size = read_u32_be(&buf[4]);
      head->comp_offset = read_u32_be(&buf[8]);
      head->uncomp_offset = read_u32_be(&buf[12]);
      return 1;
   }
   return 0;
}

void gfx_texture_cache_invalidate(void *orig_addr);

// --- Platform-independent endian swap ---
static inline uint32_t swap32(uint32_t x) {
    return ((x & 0xFF000000) >> 24) |
           ((x & 0x00FF0000) >> 8)  |
           ((x & 0x0000FF00) << 8)  |
           ((x & 0x000000FF) << 24);
}

static inline uint16_t swap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

// --- Optimized MIO0 decoder ---
//void mio0_decode(const uint8_t *src, uint8_t *dst) {
int mio0decode(const unsigned char *src, unsigned char *dst) {
    if (memcmp(src, "MIO0", 4) != 0) {
        fprintf(stderr, "Invalid MIO0 magic!\n");
        return -2;
    }
    int count = 0;
    uint32_t out_size = swap32(*(uint32_t*)(src + 4));
    uint32_t ctrl_off = swap32(*(uint32_t*)(src + 8));
    uint32_t raw_off  = swap32(*(uint32_t*)(src +12));
    uint32_t back_off = swap32(*(uint32_t*)(src +16));

    const uint8_t *ctrl = src + ctrl_off;
    const uint8_t *raw  = src + raw_off;
    const uint8_t *back = src + back_off;

    uint8_t *out = dst;
    uint8_t *out_end = dst + out_size;

    uint32_t ctrl_bits = 0;
    int bits_left = 0;

    while (out < out_end) {
        if (bits_left == 0) {
            ctrl_bits = swap32(*(uint32_t*)ctrl);
            ctrl += 4;
            bits_left = 32;
        }

        if (ctrl_bits & 0x80000000) {
            // Literal byte
            *out++ = *raw++;
            count++;
        } else {
            // Backreference
            uint16_t pair = swap16(*(uint16_t*)back);
            back += 2;

            uint16_t offset = pair >> 4;
            uint16_t length = (pair & 0xF) + 3;

            uint8_t *ref = out - offset;
            for (int i = 0; i < length; ++i)
                *out++ = *ref++;
            count += length;
         }

        ctrl_bits <<= 1;
        bits_left--;
    }
    return count;
}

#if 0
int mio0decode(const unsigned char *in, unsigned char *out) 
{
   mio0_header_t head;
   unsigned int bytes_written = 0;
   int bit_idx = 0;
   int comp_idx = 0;
   int uncomp_idx = 0;
   int valid;
//	printf("%s(%08x,%08x)%s",__func__,in,out,"\n");
//   gfx_texture_cache_invalidate(out);
   // extract header
   valid = mio0_decode_header(in, &head);
   // verify MIO0 header
   if (!valid) {
      printf("invalid header aborting from libmio0?\n");
      printf("\n");
     // while(1){}
      exit(-1);
      return -2;
   }

   // decode data
   while (bytes_written < head.dest_size) {
      if (GET_BIT(&in[MIO0_HEADER_LENGTH], bit_idx)) {
         // 1 - pull uncompressed data
         out[bytes_written] = in[head.uncomp_offset + uncomp_idx];
         bytes_written++;
         uncomp_idx++;
      } else {
         // 0 - read compressed data
         int idx;
         int length;
         int i;
         const unsigned char *vals = &in[head.comp_offset + comp_idx];
         comp_idx += 2;
         length = ((vals[0] & 0xF0) >> 4) + 3;
         idx = ((vals[0] & 0x0F) << 8) + vals[1] + 1;
         for (i = 0; i < length; i++) {
            out[bytes_written] = out[bytes_written - idx];
            bytes_written++;
         }
      }
      bit_idx++;
   }

   return bytes_written;
}
#endif