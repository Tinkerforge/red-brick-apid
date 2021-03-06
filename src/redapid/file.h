/*
 * redapid
 * Copyright (C) 2014-2016, 2019 Matthias Bolte <matthias@tinkerforge.com>
 *
 * file.h: File object implementation
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

#ifndef REDAPID_FILE_H
#define REDAPID_FILE_H

#include <stdbool.h>
#include <sys/stat.h>

#include <daemonlib/io.h>
#include <daemonlib/packet.h>
#include <daemonlib/pipe.h>

#include "object.h"
#include "string.h"

typedef enum { // bitmask
	FILE_FLAG_READ_ONLY    = 0x0001,
	FILE_FLAG_WRITE_ONLY   = 0x0002,
	FILE_FLAG_READ_WRITE   = 0x0004,
	FILE_FLAG_APPEND       = 0x0008,
	FILE_FLAG_CREATE       = 0x0010,
	FILE_FLAG_EXCLUSIVE    = 0x0020,
	FILE_FLAG_NON_BLOCKING = 0x0040,
	FILE_FLAG_TRUNCATE     = 0x0080,
	FILE_FLAG_TEMPORARY    = 0x0100, // can only be used in combination with FILE_FLAG_CREATE | FILE_FLAG_EXCLUSIVE
	FILE_FLAG_REPLACE      = 0x0200  // can only be used in combination with FILE_FLAG_CREATE
} FileFlag;

#define FILE_FLAG_ALL (FILE_FLAG_READ_ONLY | \
                       FILE_FLAG_WRITE_ONLY | \
                       FILE_FLAG_READ_WRITE | \
                       FILE_FLAG_APPEND | \
                       FILE_FLAG_CREATE | \
                       FILE_FLAG_EXCLUSIVE | \
                       FILE_FLAG_NON_BLOCKING | \
                       FILE_FLAG_TRUNCATE | \
                       FILE_FLAG_TEMPORARY | \
                       FILE_FLAG_REPLACE)

#define PIPE_FLAG_ALL (PIPE_FLAG_NON_BLOCKING_READ | \
                       PIPE_FLAG_NON_BLOCKING_WRITE)

typedef enum { // bitmask
	FILE_PERMISSION_USER_READ      = 00400,
	FILE_PERMISSION_USER_WRITE     = 00200,
	FILE_PERMISSION_USER_EXECUTE   = 00100,
	FILE_PERMISSION_GROUP_READ     = 00040,
	FILE_PERMISSION_GROUP_WRITE    = 00020,
	FILE_PERMISSION_GROUP_EXECUTE  = 00010,
	FILE_PERMISSION_OTHERS_READ    = 00004,
	FILE_PERMISSION_OTHERS_WRITE   = 00002,
	FILE_PERMISSION_OTHERS_EXECUTE = 00001
} FilePermission;

#define FILE_PERMISSION_USER_ALL (FILE_PERMISSION_USER_READ | \
                                  FILE_PERMISSION_USER_WRITE | \
                                  FILE_PERMISSION_USER_EXECUTE)

#define FILE_PERMISSION_GROUP_ALL (FILE_PERMISSION_GROUP_READ | \
                                   FILE_PERMISSION_GROUP_WRITE | \
                                   FILE_PERMISSION_GROUP_EXECUTE)

#define FILE_PERMISSION_OTHERS_ALL (FILE_PERMISSION_OTHERS_READ | \
                                    FILE_PERMISSION_OTHERS_WRITE | \
                                    FILE_PERMISSION_OTHERS_EXECUTE)

#define FILE_PERMISSION_ALL (FILE_PERMISSION_USER_ALL | \
                             FILE_PERMISSION_GROUP_ALL | \
                             FILE_PERMISSION_OTHERS_ALL)

typedef enum {
	FILE_ORIGIN_BEGINNING = 0,
	FILE_ORIGIN_CURRENT,
	FILE_ORIGIN_END
} FileOrigin;

typedef enum { // bitmask
	FILE_EVENT_READABLE = 0x0001,
	FILE_EVENT_WRITABLE = 0x0002
} FileEvent;

#define FILE_EVENT_ALL (FILE_EVENT_READABLE | \
                        FILE_EVENT_WRITABLE)

typedef enum {
	FILE_TYPE_UNKNOWN = 0,
	FILE_TYPE_REGULAR,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_CHARACTER,
	FILE_TYPE_BLOCK,
	FILE_TYPE_FIFO, // named pipe
	FILE_TYPE_SYMLINK,
	FILE_TYPE_SOCKET,
	FILE_TYPE_PIPE // unnamed pipe
} FileType;

#define FILE_MAX_READ_BUFFER_LENGTH 62
#define FILE_MAX_READ_ASYNC_BUFFER_LENGTH 60
#define FILE_MAX_WRITE_BUFFER_LENGTH 61
#define FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH 61
#define FILE_MAX_WRITE_ASYNC_BUFFER_LENGTH 61

typedef struct _File File;

typedef int (*FileReadFunction)(File *file, void *buffer, int length);
typedef int (*FileWriteFunction)(File *file, void *buffer, int length);
typedef off_t (*FileSeekFunction)(File *file, off_t offset, int whence);

struct _File {
	Object base;

	FileType type;
	String *name; // only supported if type != FILE_TYPE_PIPE
	uint32_t flags; // refers to PipeFlag if type == FILE_TYPE_PIPE,
	                // refers to FileFlag otherwise
	uint16_t events;
	IOHandle fd; // only opened if type != FILE_TYPE_PIPE
	Pipe pipe; // only created if type == FILE_TYPE_PIPE
	IOHandle async_read_eventfd;
	Pipe async_read_pipe; // only created if type == FILE_TYPE_REGULAR
	bool async_read_in_progress;
	uint64_t length_to_read_async;
	FileWriteFunction read;
	FileWriteFunction write;
	FileSeekFunction seek;
};

mode_t file_get_mode_from_permissions(uint16_t permissions);

APIE file_open(ObjectID name_id, uint32_t flags, uint16_t permissions,
               uint32_t uid, uint32_t gid, Session *session,
               uint16_t object_create_flags, ObjectID *id, File **object);

APIE pipe_create_(uint32_t flags, uint64_t length, Session *session,
                  uint16_t object_create_flags, ObjectID *id, File **object);

APIE file_get_info(File *file, Session *session, uint8_t *type,
                   ObjectID *name_id, uint32_t *flags,
                   uint16_t *permissions, uint32_t *uid, uint32_t *gid,
                   uint64_t *length, uint64_t *access_timestamp,
                   uint64_t *modification_timestamp,
                   uint64_t *status_change_timestamp);

APIE file_read_(File *file, uint8_t *buffer, uint8_t length_to_read,
                uint8_t *length_read);
PacketE file_read_async(File *file, uint64_t length_to_read);
APIE file_abort_async_read(File *file);

APIE file_write_(File *file, uint8_t *buffer, uint8_t length_to_write,
                 uint8_t *length_written);
PacketE file_write_unchecked(File *file, uint8_t *buffer, uint8_t length_to_write);
PacketE file_write_async(File *file, uint8_t *buffer, uint8_t length_to_write);

APIE file_set_position(File *file, int64_t offset, FileOrigin origin,
                       uint64_t *position);
APIE file_get_position(File *file, uint64_t *position);

APIE file_set_events(File *file, uint16_t events);
APIE file_get_events(File *file, uint16_t *events);

IOHandle file_get_read_handle(File *file);
IOHandle file_get_write_handle(File *file);

APIE file_get_acquired(ObjectID id, const char *caller, File **file);

void file_release(File *file);

#endif // REDAPID_FILE_H
