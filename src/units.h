#include <math.h>

#ifndef CHANNELSPANNER_UNITS_H
#define CHANNELSPANNER_UNITS_H

#define SPECTRUM_FREQUENCY_MIN 10.0f

#define P_72_DB   3981.1f
#define P_60_DB   1000.0f
#define P_48_DB    251.19f
#define P_36_DB     63.096f
#define P_24_DB     15.849f
#define P_12_DB      3.9811f
#define P_6_DB       1.9953f
#define O_0_DB       1.0f
#define N_6_DB       0.50119f
#define N_12_DB      0.25119f
#define N_18_DB      0.12589f
#define N_24_DB      0.063096f
#define N_36_DB      0.015849f
#define N_48_DB      0.0039811f
#define N_60_DB      0.001f
#define N_72_DB      0.00025119f
#define N_84_DB      0.000063095f
#define N_96_DB      0.000015849f
#define N_108_DB     0.0000039811f
#define N_120_DB     0.000001f
#define N_INF_DB     0.0f

#define DBTOGAIN(x) (expf( (x) * M_LN10 * 0.05f ))
#define GAINTODB(x) (20.0f * log10f(x))

#define C0 16.3516f

#endif //CHANNELSPANNER_UNITS_H
