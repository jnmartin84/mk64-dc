#ifndef SH4ZAM_H
#define SH4ZAM_H

#include <sh4zam/shz_sh4zam.h>

#define SHZ_FSCA_RAD_FACTOR     10430.37835f

#define TRIG_ARG_SCALE 0.00009587f
#define SHZ_ANGLE(a) (((float)((uint16_t)a)) * TRIG_ARG_SCALE)

#endif