/*
 * redapid
 * Copyright (C) 2014-2015, 2019 Matthias Bolte <matthias@tinkerforge.com>
 *
 * string.h: String object implementation
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef REDAPID_STRING_H
#define REDAPID_STRING_H

#include "object.h"

#define STRING_MAX_ALLOCATE_BUFFER_LENGTH 58
#define STRING_MAX_SET_CHUNK_BUFFER_LENGTH 58
#define STRING_MAX_GET_CHUNK_BUFFER_LENGTH 63

typedef struct {
	Object base;

	char *buffer; // is always NULL-terminated
	uint32_t length; // <= INT32_MAX, excludes NULL-terminator
	uint32_t allocated; // <= INT32_MAX + 1, includes NULL-terminator
} String;

APIE string_wrap(const char *buffer, Session *session,
                 uint32_t object_create_flags, ObjectID *id, String **object);
APIE string_asprintf(Session *session, uint32_t object_create_flags,
                     ObjectID *id, String **object, const char *format, ...) ATTRIBUTE_FMT_PRINTF(5, 6);
APIE string_allocate(uint32_t reserve, char *buffer, Session *session, ObjectID *id);

APIE string_truncate(String *string, uint32_t length);
APIE string_get_length(String *string, uint32_t *length);

APIE string_set_chunk(String *string, uint32_t offset, char *buffer);
APIE string_get_chunk(String *string, uint32_t offset, char *buffer);

APIE string_get(ObjectID id, const char *caller, String **string);
APIE string_get_acquired_and_locked(ObjectID id, const char *caller, String **string);

void string_acquire_and_lock(String *string);
void string_unlock_and_release(String *string);

#endif // REDAPID_STRING_H
