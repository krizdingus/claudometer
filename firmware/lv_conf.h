// firmware/lv_conf.h
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_MEM_SIZE (40U * 1024U)

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1

#define LV_USE_ARC      1
#define LV_USE_BAR      1
#define LV_USE_BTN      1
#define LV_USE_LABEL    1
#define LV_USE_LINE     1
#define LV_USE_TILEVIEW 1
#define LV_USE_IMG      1

#endif // LV_CONF_H
