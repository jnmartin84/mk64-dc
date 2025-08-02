#ifndef  __BUFFER_SIZES_H
#define  __BUFFER_SIZES_H

// uncompressed common data
#define COMMON_BUF_SIZE 184664

// segment 3 textures with wiggle room for ghost textures in Banshee Boardwalk
#define SEG3_BUF_SIZE 100352
// largest observed segment 4 data -- Bowser's Castle
#define SEG4_BUF_SIZE 228656
// largest *tex.bin -- Luigi's Raceway
#define SEG5_BUF_SIZE 133120

// the other area of memory that seg3 points at sometimes
#define OTHER_BUF_SIZE 0x1EE80

// sizeof(CeremonyActor) * 200
#define CEREMONY_ACTOR_BUF_SIZE 15200

#define COURSE_BUF_SIZE 146464
#define UNPACK_BUF_SIZE 51008
#define CEREMONY_BUF_SIZE 36232
#define COMP_VERT_BUF_SIZE 65536
#define DECOMP_VERT_BUF_SIZE 228656

#define colls_SIZE 16384
#define allColTris_SIZE 2800

#define OUTLINE_BUF_COUNT 0x14
#define OUTLIZE_BUF_SIZE (128*96/2)
#define MENUTEX_BUF_SIZE 0x900B0
#define MENUCOMP_BUF_SIZE 65536
#define LOWRES_BUF_SIZE (320*240)
#define SOMEDL_BUF_SIZE 0x1000

#define SAMPLES_HIGH 448
#define AUDIOBUF_SIZE (SAMPLES_HIGH * 2 * 2)

#endif //__BUFFER_SIZES_H