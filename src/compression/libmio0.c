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
extern void gfx_texture_cache_invalidate(void *addr);

int mio0decode(const unsigned char *in, unsigned char *out) 
{
   mio0_header_t head;
   unsigned int bytes_written = 0;
   int bit_idx = 0;
   int comp_idx = 0;
   int uncomp_idx = 0;
   int valid;
//	printf("%s(%08x,%08x)%s",__func__,in,out,"\n");
   // extract header
#if 0
   valid = mio0_decode_header(in, &head);
   // verify MIO0 header
   if (!valid) {
      printf("invalid header aborting from libmio0?\n");
      printf("\n");
     // while(1){}
      exit(-1);
      return -2;
   }
#else
   mio0_decode_header(in, &head);
#endif
   gfx_texture_cache_invalidate(out);

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

void mio0decode_noinval(const unsigned char *in, unsigned char *out) 
{
   mio0_header_t head;
   unsigned int bytes_written = 0;
   int bit_idx = 0;
   int comp_idx = 0;
   int uncomp_idx = 0;
   //int valid;
//	printf("%s(%08x,%08x)%s",__func__,in,out,"\n");
//   gfx_texture_cache_invalidate(out);
   // extract header
   /* valid = */ mio0_decode_header(in, &head);
   // verify MIO0 header
   /* if (!valid) {
      printf("invalid header aborting from libmio0?\n");
      printf("\n");
      exit(-1);
   } */

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
}
