/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Filip Matijević <filip.matijevic.pz@gmail.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib.h>

#include "up-native.h"

#include "up-bme-native.h"

G_DEFINE_TYPE (UpBmeNative, up_bme_native, G_TYPE_OBJECT)

static void
up_bme_native_class_init (UpBmeNativeClass *klass)
{
}

static void
up_bme_native_init (UpBmeNative *self)
{
	self->path = (gchar*) "empty";
}

UpBmeNative *
up_bme_native_new(const gchar * path)
{
	UpBmeNative *native;
	native = UP_BME_NATIVE (g_object_new (UP_TYPE_BME_NATIVE, NULL));
	native->path = g_strdup(path);
	return native;
}

const gchar *
up_bme_native_get_path(UpBmeNative * native)
{
	return native->path;
}

/**
 * up_native_get_native_path:
 * @object: the native tracking object
 *
 * This converts a GObject used as the device data into a native path.
 *
 * Return value: The native path for the device which is unique, e.g. "/sys/class/power/BAT1"
 **/
const gchar *
up_native_get_native_path (GObject *object)
{
	return up_bme_native_get_path (UP_BME_NATIVE (object));
}

