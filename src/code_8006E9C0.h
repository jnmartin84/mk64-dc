#ifndef CODE_8006E9C0_H
#define CODE_8006E9C0_H

#include <common_structs.h>
#include "update_objects.h"

void init_hud(void);
void reset_object_variable(void);
void func_8006EB10(void);
void clear_object_list(void);
u8* dma_copy_base_misc_textures(u8*, u8*, u32, u32);
void load_mario_kart_64_logo(void);
void init_item_window(s32);
void func_8006EEE8(s32);
void func_8006EF60(void);
void track_minimap_settings(void);
void func_8006F824(s32);
void func_8006F8CC(void);
void reset_all_kinds_of_stuff(void);
void func_80070148(void);
void init_object_list_index(void);
void init_cloud_object(s32, s32, CloudData*);
void init_clouds(CloudData*);
void init_star_object(s32, s32, StarData*);
void init_stars(StarData*);
void course_init_cloud(void);
void func_80070714(void);
void init_course_objects(void);
void init_hud_one_player(void);
void init_hud_two_player_vertical(void);
void init_hud_three_four_player(void);
void init_hud_two_player_horizontal(void);

extern s16 D_800E5520[];
extern s16 D_800E5548[];
extern u8* gCourseOutlineTextures[0x14]; // 800e54d0

#endif
