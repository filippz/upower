#ifndef _BATTERY_BPEIPC_H_
#define _BATTERY_BPEIPC_H_

/*
 * Battery management entity IPC client API
 *
 * Copyright (C) 2012 Jolla Ltd.
 * Contact: Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */


#include <stdint.h>
#include <sys/inotify.h>

#ifndef COMPILE_TIME_ASSERT
#define COMPILE_TIME_ASSERT(cond, err_msg)      \
	typedef char ERROR_##err_msg[42/(!!(cond))]
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef BIT
#define BIT(nr) (1 << (nr))
#else
#error "Check BIT macro functionality corresponds to one defined above"
#endif

/* event bit mask */
#define BME_EV_NONE 0
#define BME_EV_ERR BIT(0)
#define BME_EV_TIMEOUT BIT(1)
#define BME_EV_EMHAL BIT(2)
#define BME_EV_GENERIC BIT(3)
#define BME_EV_SYS BIT(4)
#define BME_EV_CHARGE BIT(5)
#define BME_EV_CHARGER BIT(6)
#define BME_EV_BAT BIT(7)
#define BME_EV_PSM BIT(8)
#define BME_EV_ADC BIT(9)
#define BME_EV_THERMAL BIT(10)

typedef enum {
    bme_charging_state_stopped = 0,
    bme_charging_state_started,
    bme_charging_state_special,
    bme_charging_state_err

} bme_charging_state;

typedef enum {
    bme_bat_state_empty = 0,
    bme_bat_state_low,
    bme_bat_state_ok,
    bme_bat_state_full,
    bme_bat_state_err,
} bme_bat_state;

typedef enum {
    bme_charger_state_disconnected = 0,
    bme_charger_state_connected,
    bme_charger_state_err
} bme_charger_state;

typedef enum {
    bme_stat_flags = 0,
    bme_stat_charger_state,
    bme_stat_charger_type,
    bme_stat_charging_state,
    bme_stat_charging_type,
    bme_stat_charging_time_left_min,
    bme_stat_bat_state,
    bme_stat_bat_type,
    bme_stat_bat_units_max,
    bme_stat_bat_units_now,
    bme_stat_bat_time_idle, /* h, deprecated */
    bme_stat_bat_time_left, /* min, deprecated */
    bme_stat_bat_mah_design,
    bme_stat_bat_mah_now,
    bme_stat_bat_mv_max,
    bme_stat_bat_mv_now,
    bme_stat_bat_pct_remain,
    bme_stat_bat_tk,
    bme_stat_bat_i_ma,
    bme_stat_bat_cc,
    bme_stat_bat_cc_full,
    bme_stat_system_state,
    bme_stat_bat_cond,

    bme_stat_ids_end = 32, /* reserved */
} bme_bmestat_id;

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t bme_stat_t[bme_stat_ids_end];

int bme_open(void);
int bme_close(int);

int bme_stat_get(int fd, bme_stat_t *pstat);

struct bme_xchg_desc;
typedef struct bme_xchg_desc * bme_xchg_t;

#define BME_XCHG_INVAL (NULL)

bme_xchg_t bme_xchg_open();
void bme_xchg_close(bme_xchg_t);

int bme_xchg_inotify_desc(bme_xchg_t);
int bme_xchg_inotify_read(bme_xchg_t, struct inotify_event *ev);
int bme_xchg_read(bme_xchg_t);
int bme_inotify_watch_add(bme_xchg_t);
int bme_inotify_watch_rm(bme_xchg_t);

#ifdef __cplusplus
}
#endif

#endif // _BATTERY_BPEIPC_H_

