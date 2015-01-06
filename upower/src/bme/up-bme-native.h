/***************************************************************************
 *
 * Battery management entity backend
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
 **************************************************************************/

#ifndef _UP_BACKEND_BME_H
#define _UP_BACKEND_BME_H

#include "bmeipc.h"

#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/eventfd.h>

#include "up-backend.h"


G_BEGIN_DECLS

#define UP_TYPE_BME_NATIVE		(up_bme_native_get_type ())
#define UP_BME_NATIVE(o)	   	(G_TYPE_CHECK_INSTANCE_CAST ((o), UP_TYPE_BME_NATIVE, UpBmeNative))
#define UP_BME_NATIVE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), UP_TYPE_BME_NATIVE, UpBmeNativeClass))
#define UP_IS_BME_NATIVE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), UP_TYPE_BME_NATIVE))
#define UP_IS_BME_NATIVE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), UP_TYPE_BME_NATIVE))
#define UP_BME_NATIVE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), UP_TYPE_BME_NATIVE, UpBmeNativeClass))

typedef struct
{
	GObject	parent;
	gchar*	path;
} UpBmeNative;

typedef struct
{
	GObjectClass	parent_class;
} UpBmeNativeClass;

UpBmeNative* up_bme_native_new (const char*);
const gchar * up_bme_native_get_path(UpBmeNative*);
GType up_bme_native_get_type (void);

G_END_DECLS

#endif /* _UP_BACKEND_BME_H */
