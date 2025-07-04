#!/bin/sh

mkdir -p dc_data

cp build/us/assets/code/common_data/common_data.bin dc_data

cp bin/audiobanks.us.bin dc_data/audiobanks.bin
cp bin/audiotables.bin dc_data

sh-elf-objcopy -O binary --only-section=.data build/us/data/sound_data/instrument_sets.o dc_data/instrument_sets.bin
sh-elf-objcopy -O binary --only-section=.data build/us/data/sound_data/sequences.o dc_data/sequences.bin

for i in mario_raceway choco_mountain bowsers_castle banshee_boardwalk yoshi_valley frappe_snowland koopa_troopa_beach royal_raceway luigi_raceway moo_moo_farm toads_turnpike kalimari_desert sherbet_land rainbow_road wario_stadium block_fort skyscraper double_deck dks_jungle_parkway big_donut; do cp build/us/courses/"$i"/course_data.bin dc_data/"$i"_data.bin; done

for i in mario_raceway choco_mountain bowsers_castle banshee_boardwalk yoshi_valley frappe_snowland koopa_troopa_beach royal_raceway luigi_raceway moo_moo_farm toads_turnpike kalimari_desert sherbet_land rainbow_road wario_stadium block_fort skyscraper double_deck dks_jungle_parkway big_donut; do     filename=build/us/courses/"$i"/course_offsets.o;     dec_size=$(sh-elf-size "$filename" | awk 'NR==2 {print $4}');     sh-elf-objcopy --add-symbol _course_"$i"_offsetsSegmentRomStart=.data:0x0 "$filename";     sh-elf-objcopy --add-symbol _course_"$i"_offsetsSegmentRomEnd=.data:"$dec_size" "$filename"; done

for i in mario_raceway choco_mountain bowsers_castle banshee_boardwalk yoshi_valley frappe_snowland koopa_troopa_beach royal_raceway luigi_raceway moo_moo_farm toads_turnpike kalimari_desert sherbet_land rainbow_road wario_stadium block_fort skyscraper double_deck dks_jungle_parkway big_donut; do
sh-elf-objcopy -O binary --only-section=.data build/us/courses/"$i"/course_textures.linkonly.o dc_data/"$i"_tex.bin; done

for i in mario_raceway choco_mountain bowsers_castle banshee_boardwalk yoshi_valley frappe_snowland koopa_troopa_beach royal_raceway luigi_raceway moo_moo_farm toads_turnpike kalimari_desert sherbet_land rainbow_road wario_stadium block_fort skyscraper double_deck dks_jungle_parkway big_donut; do
sh-elf-objcopy -O binary --only-section=.data build/us/courses/"$i"/course_geography.mio0.o dc_data/"$i"_geography.bin; done
