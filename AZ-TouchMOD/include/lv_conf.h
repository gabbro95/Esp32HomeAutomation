#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   Graphical settings
 ====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_HOR_RES 320
#define LV_VER_RES 240

/* Enable/disable the touch input device */
#define LV_USE_LOG 1
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/*====================
   Drivers
 ====================*/
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0

/*====================
   Others
 ====================*/
#define LV_USE_USER_DATA 1

#endif /*LV_CONF_H*/
