#!/bin/sh
rm build/us/mk64.us.*
rm build/us/src/*.o
rm build/us/src/audio/*.o
rm build/us/src/data/*.o
rm build/us/src/debug/*.o
rm build/us/src/ending/*.o
rm build/us/src/os/*.o
rm build/us/src/racing/*.o

make -f fuckedMakefile

kos-cc -c  -DTARGET_DC -D_LANGUAGE_C  -Iinclude -IlibGL-1.1.0/include -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion -DENABLE_OPENGL -Os -o build/us/src/gfx/gfx_cc.o src/gfx/gfx_cc.c
kos-cc -c  -DTARGET_DC -D_LANGUAGE_C  -Iinclude -IlibGL-1.1.0/include -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion -DENABLE_OPENGL -Os -o build/us/src/gfx/gfx_dc.o src/gfx/gfx_dc.c
kos-cc -c  -DTARGET_DC -D_LANGUAGE_C  -Iinclude -IlibGL-1.1.0/include -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion -DENABLE_OPENGL  -Os -o build/us/src/gfx/gfx_gldc.o src/gfx/gfx_gldc.c
kos-cc -c  -DTARGET_DC -D_LANGUAGE_C  -Iinclude -IlibGL-1.1.0/include -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion -DENABLE_OPENGL -Os -o build/us/src/gfx/gfx_retro_dc.o src/gfx/gfx_retro_dc.c
kos-cc -c  -DTARGET_DC -D_LANGUAGE_C  -Iinclude -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion -fno-data-sections -DENABLE_OPENGL  -Os -o build/us/src/dcaudio/driver.o src/dcaudio/driver.c
kos-cc -c  -DTARGET_DC -D_LANGUAGE_C  -Iinclude -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion -fno-data-sections -DENABLE_OPENGL -Os -o libmio0.o libmio0.c
kos-cc -c  -DTARGET_DC -D_LANGUAGE_C  -Iinclude -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion -fno-data-sections -DENABLE_OPENGL -Os -o libtkmk00.o libtkmk00.c
make -f dcMakefile
