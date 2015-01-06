/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015
 * Contact: Filip Matijević <filip.matijevic.pz@gmail.com>
 * 
 * Copyright (C) 2013 0x7DD.
 * Contact: Andrey Kozhevnikov <coderusinbox@gmail.com>
 *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 *
 * Copyright (C) 2010 Nokia Corporation.
 * Contact: Marius Vollmer marius.vollmer@nokia.com 
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "up-bme-native.h"
#include "bmeipc.h"

#include "up-backend.h"
#include "up-daemon.h"
#include "up-marshal.h"

#include <time.h>

static void	up_backend_class_init	(UpBackendClass	*klass);
static void	up_backend_init	(UpBackend		*backend);
static void	up_backend_finalize	(GObject		*object);

static void initialize_bme(UpBackend *backend);
static void onBMEEvent(UpBackend *backend);
static gboolean up_backend_bme_event_cb(gpointer object);
static gboolean readBatteryValues(bme_stat_t *stat);


static gboolean		up_bme_device_get_on_battery	(UpDevice *device, gboolean *on_battery);
static gboolean		up_bme_device_get_low_battery	(UpDevice *device, gboolean *low_battery);
static gboolean		up_bme_device_get_online	(UpDevice *device, gboolean *online);
static gboolean		up_bme_device_refresh		(UpDevice *device);

#define UP_BACKEND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_BACKEND, UpBackendPrivate))

struct UpBackendPrivate
{
	UpDaemon		*daemon;
	UpDevice		*battery;
	UpDevice		*usb;
	GThread			*bme_thread;
	bme_xchg_t		xchg;
	int			desc;
	gboolean		exiting;
	struct pollfd 		ufds[2];
};

enum {
	SIGNAL_DEVICE_ADDED,
	SIGNAL_DEVICE_REMOVED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (UpBackend, up_backend, G_TYPE_OBJECT)

static gpointer
up_backend_bme_event_thread(gpointer object)
{
	UpBackend *backend;
	int rv = 0;

	g_return_val_if_fail (UP_IS_BACKEND (object), NULL);
	backend = UP_BACKEND (object);
	
	while (!backend->priv->exiting) {
		rv = poll(backend->priv->ufds, 2, -1);
		if (rv < 0) {
			g_warning ("poll error");
                
			usleep(10000000);
			initialize_bme(backend);
		} else if (rv == 0) {
			g_warning ("Timeout");
		} else {
			if (backend->priv->ufds[1].revents & POLLIN) { // exit event
				break;
			}
			if (backend->priv->ufds[0].revents & POLLIN) { // bme event
				onBMEEvent(backend);
			}
		}
	}

	return NULL;
}

/**
 * up_backend_coldplug:
 * @backend: The %UpBackend class instance
 * @daemon: The %UpDaemon controlling instance
 *
 * Finds all the devices already plugged in, and emits device-add signals for
 * each of them.
 *
 * Return value: %TRUE for success
 **/
gboolean
up_backend_coldplug (UpBackend *backend, UpDaemon *daemon)
{
	UpBmeNative *batterynative = NULL;
	UpBmeNative *usbnative = NULL;

	backend->priv->daemon = g_object_ref (daemon);

	usbnative = up_bme_native_new("usb");
	if (!up_device_coldplug (backend->priv->usb, backend->priv->daemon, G_OBJECT(usbnative)))
		g_warning ("failed to coldplug usb");
	else
		g_signal_emit (backend, signals[SIGNAL_DEVICE_ADDED], 0, usbnative, backend->priv->usb);

	batterynative = up_bme_native_new("battery");
	if (!up_device_coldplug (backend->priv->battery, backend->priv->daemon, G_OBJECT(batterynative)))
		g_warning ("failed to coldplug battery");
	else
		g_signal_emit (backend, signals[SIGNAL_DEVICE_ADDED], 0, batterynative, backend->priv->battery);

	return TRUE;
}

void initialize_bme(UpBackend *backend)
{
	backend->priv->xchg = bme_xchg_open();
	if (backend->priv->xchg == BME_XCHG_INVAL) {
		return;
	}

	fcntl(bme_xchg_inotify_desc(backend->priv->xchg), F_SETFD, FD_CLOEXEC);
	backend->priv->desc = bme_xchg_inotify_desc(backend->priv->xchg);
	if (backend->priv->desc >= 0) {
		backend->priv->ufds[0].fd = backend->priv->desc;
		backend->priv->ufds[0].events = POLLIN;
	}
	else {
		backend->priv->exiting = TRUE;
	}
}

void onBMEEvent(UpBackend *backend)
{
	struct inotify_event ev;
	int rc;
	rc = bme_xchg_inotify_read(backend->priv->xchg, &ev);
	if (rc < 0) {
		g_warning("can't read bmeipc xchg inotify event");
		return;
	}

	if ((ev.mask & IN_DELETE_SELF) || (ev.mask & IN_MOVE_SELF)) {
		bme_inotify_watch_rm(backend->priv->xchg);
		bme_inotify_watch_add(backend->priv->xchg);
	} else if (!(ev.mask & IN_IGNORED)) {
		g_idle_add((GSourceFunc) up_backend_bme_event_cb, backend);
	}
}

gboolean readBatteryValues(bme_stat_t *stat)
{
	int sd = -1;

	if ((sd = bme_open()) < 0) {
		g_warning("Cannot open socket connected to BME server");
		return FALSE;
	}

	if (bme_stat_get(sd, stat) < 0) {
		g_warning("Cannot get BME statistics");
		bme_close(sd);
		return FALSE;
	}

	bme_close(sd);

	return TRUE;
}

/* callback updating the device */
gboolean
up_backend_bme_event_cb(gpointer object)
{
	UpBackend *backend;

	g_return_val_if_fail (UP_IS_BACKEND (object), FALSE);
	backend = UP_BACKEND (object);
	up_bme_device_refresh(backend->priv->usb);
	up_bme_device_refresh(backend->priv->battery);
	/* return false to not endless loop */
	return FALSE;
}
static UpDeviceState
up_backend_bme_get_battery_state_value(int32_t bme_charger_state, int32_t bme_battery_state) {

	if ((bme_charger_state == bme_charging_state_started) && (bme_battery_state != bme_bat_state_full))
		return UP_DEVICE_STATE_CHARGING;

	switch(bme_battery_state) {
		case bme_bat_state_empty:
			return UP_DEVICE_STATE_EMPTY;
		case bme_bat_state_low:
		case bme_bat_state_ok:
			return UP_DEVICE_STATE_DISCHARGING;
		case bme_bat_state_full:
			return UP_DEVICE_STATE_FULLY_CHARGED;
		case bme_bat_state_err:
			return UP_DEVICE_STATE_UNKNOWN;
	}

	return -1;
}

static gboolean
up_backend_update_battery_state(UpDevice* device)
{
	gboolean ret, is_online;
	UpDeviceState state;
	gdouble percentage, temperature, energy, energy_full, energy_full_design, energy_rate, voltage, capacity;
	gint64 time_to_empty, time_to_full;
	bme_stat_t st;

	ret = readBatteryValues(&st);
	if (!ret)
		return ret;

	is_online = (st[bme_stat_charger_state] == bme_charging_state_started
				&& st[bme_stat_bat_state] != bme_bat_state_full ? TRUE : FALSE);
	state = up_backend_bme_get_battery_state_value(st[bme_stat_charger_state], st[bme_stat_bat_state]);
	percentage = st[bme_stat_bat_pct_remain];
	time_to_empty = st[bme_stat_bat_time_left] * 60;
	time_to_full = st[bme_stat_charging_time_left_min] * 60;
	temperature = st[bme_stat_bat_tk] - 273.15f; /*Kelvin to Centigrade*/
	voltage = st[bme_stat_bat_mv_now] / 1000.0f;
	energy = st[bme_stat_bat_mah_now] * voltage;
	energy_full_design = st[bme_stat_bat_mah_design] * st[bme_stat_bat_mv_max] / 1000.0f;
	energy_rate = st[bme_stat_bat_i_ma] * voltage;	
	/*When chargin this is negative*/
	if (st[bme_stat_bat_i_ma]<0)
		energy_rate = -energy_rate;

	if (st[bme_stat_bat_cc_full] == 0) {
		/*
		 * There are N9 with aftermarket batteries being detected as LI4V2 instead of LI4V35
		 * Their capacity is being detected as 1200mAh vs 1450mAh, and max voltage of 4200mV vs 4350mV
		 * BME reports last full capacity of 0mAh for them
		 */
		energy_full = energy_full_design;
		capacity = 100.0f;
	}
	else {
		energy_full = st[bme_stat_bat_cc_full] * st[bme_stat_bat_mv_max] / 1000.0f;
		capacity = (double)st[bme_stat_bat_cc_full] / st[bme_stat_bat_mah_design];
	}

	g_object_set (device,
		      "online", is_online,
		      "state", state,
		      "percentage", percentage,
		      "time_to_empty", time_to_empty,
		      "time_to_full", time_to_full,
		      "temperature", temperature,
		      "energy", energy,
		      "energy-full", energy_full,
		      "energy-full-design",energy_full_design,
		      "energy-rate", energy_rate,
		      "voltage", voltage,
		      "capacity", capacity,
		      (void*) NULL);

	return TRUE;
}

static gboolean
up_backend_update_usb_state(UpDevice* device)
{
	gboolean ret, is_online;
	bme_stat_t st;

	ret = readBatteryValues(&st);
	if (!ret)
		return ret;

	is_online = (st[bme_stat_charger_state] == bme_charger_state_connected ? TRUE : FALSE);
	g_object_set (device,
		      "online", is_online,
		      (void*) NULL);

	return TRUE;
}

static gboolean
up_bme_device_refresh(UpDevice* device)
{
	UpDeviceKind type;
	GTimeVal timeval;
	gboolean ret;
	g_object_get (device, "type", &type, NULL);
	
	switch (type) {
		case UP_DEVICE_KIND_LINE_POWER:
			ret = up_backend_update_usb_state(device);
			break;
		case UP_DEVICE_KIND_BATTERY:
			ret = up_backend_update_battery_state(device);
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	if (ret) {
		g_get_current_time (&timeval);
		g_object_set (device, "update-time", (guint64) timeval.tv_sec, NULL);
	}

	return ret;
}

gboolean
up_bme_device_get_on_battery (UpDevice *device, gboolean * on_battery)
{
	UpDeviceKind type;
	UpDeviceState state;
	gboolean is_present;

	g_return_val_if_fail (on_battery != NULL, FALSE);

	g_object_get (device,
		      "type", &type,
		      "state", &state,
		      "is-present", &is_present,
		      (void*) NULL);

	if (type != UP_DEVICE_KIND_BATTERY)
		return FALSE;
	if (state == UP_DEVICE_STATE_UNKNOWN)
		return FALSE;
	if (!is_present)
		return FALSE;

	*on_battery = (state == UP_DEVICE_STATE_DISCHARGING);
	return TRUE;
}
gboolean
up_bme_device_get_low_battery (UpDevice *device, gboolean * low_battery)
{
	gboolean ret;
	gboolean on_battery;
	bme_stat_t st;

	g_return_val_if_fail (low_battery != NULL, FALSE);

	ret = up_bme_device_get_on_battery (device, &on_battery);
	if (!ret)
		return FALSE;

	if (!on_battery) {
		*low_battery = FALSE;
		return TRUE;
	}

	ret = readBatteryValues(&st);
	
	if (!ret)
		return FALSE;

	*low_battery = (st[bme_stat_bat_state] != bme_bat_state_low ? TRUE : FALSE);
	return TRUE;
}

gboolean
up_bme_device_get_online (UpDevice *device, gboolean * online)
{
	UpDeviceKind type;
	gboolean online_tmp;

	g_return_val_if_fail (online != NULL, FALSE);

	g_object_get (device,
		      "type", &type,
		      "online", &online_tmp,
		      (void*) NULL);

	if (type != UP_DEVICE_KIND_LINE_POWER)
		return FALSE;

	*online = online_tmp;

	return TRUE;
}

/**
 * up_backend_class_init:
 * @klass: The UpBackendClass
 **/
static void
up_backend_class_init (UpBackendClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_backend_finalize;

	signals [SIGNAL_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpBackendClass, device_added),
			      NULL, NULL, up_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
	signals [SIGNAL_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpBackendClass, device_removed),
			      NULL, NULL, up_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	g_type_class_add_private (klass, sizeof (UpBackendPrivate));
}

/**
 * up_backend_init:
 **/
static void
up_backend_init (UpBackend *backend)
{
	GError *err = NULL;
	GTimeVal timeval;
	UpDeviceClass *device_class;
	int exit_handler;

	backend->priv = UP_BACKEND_GET_PRIVATE (backend);
	backend->priv->daemon = NULL;
	backend->priv->usb = NULL;
	backend->priv->battery = NULL;
	
	memset(backend->priv->ufds, 0 , sizeof(backend->priv->ufds));

	backend->priv->exiting = FALSE;
	
	exit_handler = eventfd(0, 0);
	if (exit_handler >= 0) {
		backend->priv->ufds[1].fd = exit_handler;
		backend->priv->ufds[1].events = POLLIN;

		initialize_bme(backend);
	}

	backend->priv->usb = UP_DEVICE(up_device_new());
	device_class = UP_DEVICE_GET_CLASS (backend->priv->usb);
	device_class->get_on_battery = up_bme_device_get_on_battery;
	device_class->get_low_battery = up_bme_device_get_low_battery;
	device_class->get_online = up_bme_device_get_online;
	device_class->refresh = up_bme_device_refresh;
	backend->priv->battery = UP_DEVICE(up_device_new());
	device_class = UP_DEVICE_GET_CLASS (backend->priv->battery);
	device_class->get_on_battery = up_bme_device_get_on_battery;
	device_class->get_low_battery = up_bme_device_get_low_battery;
	device_class->get_online = up_bme_device_get_online;
	device_class->refresh = up_bme_device_refresh;

	/* creates thread */
	backend->priv->bme_thread = (GThread*) g_thread_try_new("bme-poller",(GThreadFunc)up_backend_bme_event_thread, (void*) backend, &err);
	if((backend->priv->bme_thread == NULL))
	{
		g_warning("Thread create failed: %s", err->message);
		g_error_free (err);
	}

	g_get_current_time (&timeval);
	g_object_set (backend->priv->battery,
			"type", UP_DEVICE_KIND_BATTERY,
			"power-supply", TRUE,
			"is-present", TRUE,
			"is-rechargeable", TRUE,
			"has-history", TRUE,
			"has-statistics", TRUE,
			"state", UP_DEVICE_STATE_UNKNOWN,
			"percentage", 0.0f,
			"temperature", 0.0f,
			"time-to-empty", (gint64) 0,
			"update-time", (guint64) timeval.tv_sec,
		        "technology", UP_DEVICE_TECHNOLOGY_LITHIUM_ION, /*All BME devices have LI batteries?*/
			(void*) NULL);
	g_object_set (backend->priv->usb,
			"type", UP_DEVICE_KIND_LINE_POWER,
			"online", FALSE,
			"power-supply", TRUE,
			"update-time", (guint64) timeval.tv_sec,
			(void*) NULL);
}

/**
 * up_backend_finalize:
 **/
static void
up_backend_finalize (GObject *object)
{
	UpBackend *backend;

	g_return_if_fail (UP_IS_BACKEND (object));

	backend = UP_BACKEND (object);

	if (backend->priv->daemon != NULL)
		g_object_unref (backend->priv->daemon);
	if (backend->priv->battery != NULL)
		g_object_unref (backend->priv->battery);
	if (backend->priv->usb != NULL)
		g_object_unref (backend->priv->usb);

	G_OBJECT_CLASS (up_backend_parent_class)->finalize (object);
}



/**
 * up_backend_new:
 *
 * Return value: a new %UpBackend object.
 **/
UpBackend *
up_backend_new (void)
{
	return g_object_new (UP_TYPE_BACKEND, NULL);
}