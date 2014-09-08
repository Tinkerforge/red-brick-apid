/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * api.c: RED Brick API implementation
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

#include <errno.h>
#include <string.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "api.h"

#include "directory.h"
#include "file.h"
#include "inventory.h"
#include "list.h"
#include "network.h"
#include "process.h"
#include "program.h"
#include "string.h"
#include "version.h"

#define LOG_CATEGORY LOG_CATEGORY_API

#define RED_BRICK_DEVICE_IDENTIFIER 17

// ensure that bool values in the packet definitions follow the TFP definition
// of a bool and don't rely on stdbool.h to fulfill this
typedef uint8_t tfpbool;

typedef enum {
	FUNCTION_RELEASE_OBJECT = 1,

	FUNCTION_OPEN_INVENTORY,
	FUNCTION_GET_INVENTORY_TYPE,
	FUNCTION_GET_NEXT_INVENTORY_ENTRY,
	FUNCTION_REWIND_INVENTORY,

	FUNCTION_ALLOCATE_STRING,
	FUNCTION_TRUNCATE_STRING,
	FUNCTION_GET_STRING_LENGTH,
	FUNCTION_SET_STRING_CHUNK,
	FUNCTION_GET_STRING_CHUNK,

	FUNCTION_ALLOCATE_LIST,
	FUNCTION_GET_LIST_LENGTH,
	FUNCTION_GET_LIST_ITEM,
	FUNCTION_APPEND_TO_LIST,
	FUNCTION_REMOVE_FROM_LIST,

	FUNCTION_OPEN_FILE,
	FUNCTION_CREATE_PIPE,
	FUNCTION_GET_FILE_INFO,
	FUNCTION_READ_FILE,
	FUNCTION_READ_FILE_ASYNC,
	FUNCTION_ABORT_ASYNC_FILE_READ,
	FUNCTION_WRITE_FILE,
	FUNCTION_WRITE_FILE_UNCHECKED,
	FUNCTION_WRITE_FILE_ASYNC,
	FUNCTION_SET_FILE_POSITION,
	FUNCTION_GET_FILE_POSITION,
	CALLBACK_ASYNC_FILE_READ,
	CALLBACK_ASYNC_FILE_WRITE,
	FUNCTION_LOOKUP_FILE_INFO,
	FUNCTION_LOOKUP_SYMLINK_TARGET,

	FUNCTION_OPEN_DIRECTORY,
	FUNCTION_GET_DIRECTORY_NAME,
	FUNCTION_GET_NEXT_DIRECTORY_ENTRY,
	FUNCTION_REWIND_DIRECTORY,
	FUNCTION_CREATE_DIRECTORY,

	FUNCTION_SPAWN_PROCESS,
	FUNCTION_KILL_PROCESS,
	FUNCTION_GET_PROCESS_COMMAND,
	FUNCTION_GET_PROCESS_IDENTITY,
	FUNCTION_GET_PROCESS_STDIO,
	FUNCTION_GET_PROCESS_STATE,
	CALLBACK_PROCESS_STATE_CHANGED,

	FUNCTION_DEFINE_PROGRAM,
	FUNCTION_UNDEFINE_PROGRAM,
	FUNCTION_GET_PROGRAM_IDENTIFIER,
	FUNCTION_GET_PROGRAM_DIRECTORY,
	FUNCTION_SET_PROGRAM_COMMAND,
	FUNCTION_GET_PROGRAM_COMMAND,
	FUNCTION_SET_PROGRAM_STDIO_REDIRECTION,
	FUNCTION_GET_PROGRAM_STDIO_REDIRECTION,
	FUNCTION_SET_PROGRAM_SCHEDULE,
	FUNCTION_GET_PROGRAM_SCHEDULE
} APIFunctionID;

#include <daemonlib/packed_begin.h>

//
// object
//

typedef struct {
	PacketHeader header;
	uint16_t object_id;
} ATTRIBUTE_PACKED ReleaseObjectRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReleaseObjectResponse;

//
// inventory
//

typedef struct {
	PacketHeader header;
	uint8_t type;
} ATTRIBUTE_PACKED OpenInventoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t inventory_id;
} ATTRIBUTE_PACKED OpenInventoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t inventory_id;
} ATTRIBUTE_PACKED GetInventoryTypeRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
} ATTRIBUTE_PACKED GetInventoryTypeResponse;

typedef struct {
	PacketHeader header;
	uint16_t inventory_id;
} ATTRIBUTE_PACKED GetNextInventoryEntryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t object_id;
} ATTRIBUTE_PACKED GetNextInventoryEntryResponse;

typedef struct {
	PacketHeader header;
	uint16_t inventory_id;
} ATTRIBUTE_PACKED RewindInventoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RewindInventoryResponse;

//
// string
//

typedef struct {
	PacketHeader header;
	uint32_t length_to_reserve;
	char buffer[STRING_MAX_ALLOCATE_BUFFER_LENGTH];
} ATTRIBUTE_PACKED AllocateStringRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t string_id;
} ATTRIBUTE_PACKED AllocateStringResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t length;
} ATTRIBUTE_PACKED TruncateStringRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED TruncateStringResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
} ATTRIBUTE_PACKED GetStringLengthRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint32_t length;
} ATTRIBUTE_PACKED GetStringLengthResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
	char buffer[STRING_MAX_SET_CHUNK_BUFFER_LENGTH];
} ATTRIBUTE_PACKED SetStringChunkRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetStringChunkResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
} ATTRIBUTE_PACKED GetStringChunkRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	char buffer[STRING_MAX_GET_CHUNK_BUFFER_LENGTH];
} ATTRIBUTE_PACKED GetStringChunkResponse;

//
// list
//

typedef struct {
	PacketHeader header;
	uint16_t length_to_reserve;
} ATTRIBUTE_PACKED AllocateListRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t list_id;
} ATTRIBUTE_PACKED AllocateListResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
} ATTRIBUTE_PACKED GetListLengthRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t length;
} ATTRIBUTE_PACKED GetListLengthResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t item_object_id;
} ATTRIBUTE_PACKED AppendToListRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED AppendToListResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t index;
} ATTRIBUTE_PACKED RemoveFromListRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RemoveFromListResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t index;
} ATTRIBUTE_PACKED GetListItemRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t item_object_id;
} ATTRIBUTE_PACKED GetListItemResponse;

//
// file
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint16_t flags;
	uint16_t permissions;
	uint32_t user_id;
	uint32_t group_id;
} ATTRIBUTE_PACKED OpenFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED OpenFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t flags;
} ATTRIBUTE_PACKED CreatePipeRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED CreatePipeResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileInfoRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
	uint16_t name_string_id;
	uint16_t flags;
} ATTRIBUTE_PACKED GetFileInfoResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_MAX_WRITE_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED WriteFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileUncheckedRequest;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_MAX_WRITE_ASYNC_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileAsyncRequest;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t length_to_read;
} ATTRIBUTE_PACKED ReadFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t buffer[FILE_MAX_READ_BUFFER_LENGTH];
	uint8_t length_read;
} ATTRIBUTE_PACKED ReadFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint64_t length_to_read;
} ATTRIBUTE_PACKED ReadFileAsyncRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReadFileAsyncResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED AbortAsyncFileReadRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED AbortAsyncFileReadResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	int64_t offset;
	uint8_t origin;
} ATTRIBUTE_PACKED SetFilePositionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint64_t position;
} ATTRIBUTE_PACKED SetFilePositionResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFilePositionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint64_t position;
} ATTRIBUTE_PACKED GetFilePositionResponse;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	tfpbool follow_symlink;
} ATTRIBUTE_PACKED LookupFileInfoRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
	uint16_t permissions;
	uint32_t user_id;
	uint32_t group_id;
	uint64_t length;
	uint64_t access_time;
	uint64_t modification_time;
	uint64_t status_change_time;
} ATTRIBUTE_PACKED LookupFileInfoResponse;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	tfpbool canonicalize;
} ATTRIBUTE_PACKED LookupSymlinkTargetRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t target_string_id;
} ATTRIBUTE_PACKED LookupSymlinkTargetResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];
	uint8_t length_read;
} ATTRIBUTE_PACKED AsyncFileReadCallback;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED AsyncFileWriteCallback;

//
// directory
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED OpenDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t directory_id;
} ATTRIBUTE_PACKED OpenDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED GetDirectoryNameRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetDirectoryNameResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED GetNextDirectoryEntryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
	uint8_t type;
} ATTRIBUTE_PACKED GetNextDirectoryEntryResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED RewindDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RewindDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	tfpbool recursive;
	uint16_t permissions;
	uint32_t user_id;
	uint32_t group_id;
} ATTRIBUTE_PACKED CreateDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED CreateDirectoryResponse;

//
// process
//

typedef struct {
	PacketHeader header;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
	uint16_t working_directory_string_id;
	uint32_t user_id;
	uint32_t group_id;
	uint16_t stdin_file_id;
	uint16_t stdout_file_id;
	uint16_t stderr_file_id;
} ATTRIBUTE_PACKED SpawnProcessRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t process_id;
} ATTRIBUTE_PACKED SpawnProcessResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
	uint8_t signal;
} ATTRIBUTE_PACKED KillProcessRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED KillProcessResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
	uint16_t working_directory_string_id;
} ATTRIBUTE_PACKED GetProcessCommandResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessIdentityRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint32_t user_id;
	uint32_t group_id;
} ATTRIBUTE_PACKED GetProcessIdentityResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessStdioRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t stdin_file_id;
	uint16_t stdout_file_id;
	uint16_t stderr_file_id;
} ATTRIBUTE_PACKED GetProcessStdioResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessStateRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t state;
	uint8_t exit_code;
} ATTRIBUTE_PACKED GetProcessStateResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
	uint8_t state;
	uint8_t exit_code;
} ATTRIBUTE_PACKED ProcessStateChangedCallback;

//
// program
//

typedef struct {
	PacketHeader header;
	uint16_t identifier_string_id;
} ATTRIBUTE_PACKED DefineProgramRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t program_id;
} ATTRIBUTE_PACKED DefineProgramResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED UndefineProgramRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED UndefineProgramResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramIdentifierRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t identifier_string_id;
} ATTRIBUTE_PACKED GetProgramIdentifierResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t directory_string_id;
} ATTRIBUTE_PACKED GetProgramDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
} ATTRIBUTE_PACKED SetProgramCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramCommandResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
} ATTRIBUTE_PACKED GetProgramCommandResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint8_t stdin_redirection;
	uint16_t stdin_file_name_string_id;
	uint8_t stdout_redirection;
	uint16_t stdout_file_name_string_id;
	uint8_t stderr_redirection;
	uint16_t stderr_file_name_string_id;
} ATTRIBUTE_PACKED SetProgramStdioRedirectionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramStdioRedirectionResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramStdioRedirectionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t stdin_redirection;
	uint16_t stdin_file_name_string_id;
	uint8_t stdout_redirection;
	uint16_t stdout_file_name_string_id;
	uint8_t stderr_redirection;
	uint16_t stderr_file_name_string_id;
} ATTRIBUTE_PACKED GetProgramStdioRedirectionResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint8_t start_condition;
	uint64_t start_time;
	uint32_t start_delay;
	uint8_t repeat_mode;
	uint32_t repeat_interval;
	uint64_t repeat_second_mask;
	uint64_t repeat_minute_mask;
	uint32_t repeat_hour_mask;
	uint32_t repeat_day_mask;
	uint16_t repeat_month_mask;
	uint8_t repeat_weekday_mask;
} ATTRIBUTE_PACKED SetProgramScheduleRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramScheduleResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramScheduleRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t start_condition;
	uint64_t start_time;
	uint32_t start_delay;
	uint8_t repeat_mode;
	uint32_t repeat_interval;
	uint64_t repeat_second_mask;
	uint64_t repeat_minute_mask;
	uint32_t repeat_hour_mask;
	uint32_t repeat_day_mask;
	uint16_t repeat_month_mask;
	uint8_t repeat_weekday_mask;
} ATTRIBUTE_PACKED GetProgramScheduleResponse;

//
// misc
//

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetIdentityRequest;

typedef struct {
	PacketHeader header;
	char uid[8];
	char connected_uid[8];
	char position;
	uint8_t hardware_version[3];
	uint8_t firmware_version[3];
	uint16_t device_identifier;
} ATTRIBUTE_PACKED GetIdentityResponse;

#include <daemonlib/packed_end.h>

//
// api
//

static uint32_t _uid = 0; // always little endian
static AsyncFileReadCallback _async_file_read_callback;
static AsyncFileWriteCallback _async_file_write_callback;
static ProcessStateChangedCallback _process_state_changed_callback;

static void api_prepare_response(Packet *request, Packet *response, uint8_t length) {
	// memset'ing the whole response to zero first ensures that all members
	// have a known initial value, that no random heap/stack data can leak
	// to the client and that all potential object ID members are set to
	// zero to indicate that there is no object here
	memset(response, 0, length);

	response->header.uid = request->header.uid;
	response->header.length = length;
	response->header.function_id = request->header.function_id;

	packet_header_set_sequence_number(&response->header,
	                                  packet_header_get_sequence_number(&request->header));
	packet_header_set_response_expected(&response->header, true);
}

void api_prepare_callback(Packet *callback, uint8_t length, uint8_t function_id) {
	memset(callback, 0, length);

	callback->header.uid = _uid;
	callback->header.length = length;
	callback->header.function_id = function_id;

	packet_header_set_sequence_number(&callback->header, 0);
	packet_header_set_response_expected(&callback->header, true);
}

static void api_send_response_if_expected(Packet *request, ErrorCode error_code) {
	ErrorCodeResponse response;

	if (!packet_header_get_response_expected(&request->header)) {
		return;
	}

	api_prepare_response(request, (Packet *)&response, sizeof(response));

	packet_header_set_error_code(&response.header, error_code);

	network_dispatch_response((Packet *)&response);
}

#define FORWARD_FUNCTION(packet_prefix, function_suffix, call) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		call \
		network_dispatch_response((Packet *)&response); \
	}

//
// object
//

FORWARD_FUNCTION(ReleaseObject, release_object, {
	response.error_code = object_release(request->object_id);
})

//
// inventory
//

FORWARD_FUNCTION(OpenInventory, open_inventory, {
	response.error_code = inventory_open(request->type, &response.inventory_id);
})

FORWARD_FUNCTION(GetInventoryType, get_inventory_type, {
	response.error_code = inventory_get_type(request->inventory_id,
	                                         &response.type);
})

FORWARD_FUNCTION(GetNextInventoryEntry, get_next_inventory_entry, {
	response.error_code = inventory_get_next_entry(request->inventory_id,
	                                               &response.object_id);
})

FORWARD_FUNCTION(RewindInventory, rewind_inventory, {
	response.error_code = inventory_rewind(request->inventory_id);
})

//
// string
//

FORWARD_FUNCTION(AllocateString, allocate_string, {
	response.error_code = string_allocate(request->length_to_reserve,
	                                      request->buffer, &response.string_id);
})

FORWARD_FUNCTION(TruncateString, truncate_string, {
	response.error_code = string_truncate(request->string_id, request->length);
})

FORWARD_FUNCTION(GetStringLength, get_string_length, {
	response.error_code = string_get_length(request->string_id, &response.length);
})

FORWARD_FUNCTION(SetStringChunk, set_string_chunk, {
	response.error_code = string_set_chunk(request->string_id, request->offset,
	                                       request->buffer);
})

FORWARD_FUNCTION(GetStringChunk, get_string_chunk, {
	response.error_code = string_get_chunk(request->string_id, request->offset,
	                                       response.buffer);
})

//
// list
//

FORWARD_FUNCTION(AllocateList, allocate_list, {
	response.error_code = list_allocate(request->length_to_reserve,
	                                    &response.list_id);
})

FORWARD_FUNCTION(GetListLength, get_list_length, {
	response.error_code = list_get_length(request->list_id, &response.length);
})

FORWARD_FUNCTION(GetListItem, get_list_item, {
	response.error_code = list_get_item(request->list_id, request->index,
	                                    &response.item_object_id);
})

FORWARD_FUNCTION(AppendToList, append_to_list, {
	response.error_code = list_append_to(request->list_id,
	                                     request->item_object_id);
})

FORWARD_FUNCTION(RemoveFromList, remove_from_list, {
	response.error_code = list_remove_from(request->list_id, request->index);
})

//
// file
//

FORWARD_FUNCTION(OpenFile, open_file, {
	response.error_code = file_open(request->name_string_id, request->flags,
	                                request->permissions, request->user_id,
	                                request->group_id, &response.file_id);
})

FORWARD_FUNCTION(CreatePipe, create_pipe, {
	response.error_code = pipe_create_(&response.file_id, request->flags);
})

FORWARD_FUNCTION(GetFileInfo, get_file_info, {
	response.error_code = file_get_info(request->file_id, &response.type,
	                                    &response.name_string_id, &response.flags);
})

FORWARD_FUNCTION(ReadFile, read_file, {
	response.error_code = file_read(request->file_id, response.buffer,
	                                request->length_to_read,
	                                &response.length_read);
})

FORWARD_FUNCTION(ReadFileAsync, read_file_async, {
	response.error_code = file_read_async(request->file_id,
	                                      request->length_to_read);
})

FORWARD_FUNCTION(AbortAsyncFileRead, abort_async_file_read, {
	response.error_code = file_abort_async_read(request->file_id);
})

FORWARD_FUNCTION(WriteFile, write_file, {
	response.error_code = file_write(request->file_id, request->buffer,
	                                 request->length_to_write,
	                                 &response.length_written);
})

static void api_write_file_unchecked(WriteFileUncheckedRequest *request) {
	ErrorCode error_code = file_write_unchecked(request->file_id, request->buffer,
	                                            request->length_to_write);

	api_send_response_if_expected((Packet *)request, error_code);
}

static void api_write_file_async(WriteFileAsyncRequest *request) {
	ErrorCode error_code = file_write_async(request->file_id, request->buffer,
	                                        request->length_to_write);

	api_send_response_if_expected((Packet *)request, error_code);
}

FORWARD_FUNCTION(SetFilePosition, set_file_position, {
	response.error_code = file_set_position(request->file_id, request->offset,
	                                        request->origin, &response.position);
})

FORWARD_FUNCTION(GetFilePosition, get_file_position, {
	response.error_code = file_get_position(request->file_id, &response.position);
})

FORWARD_FUNCTION(LookupFileInfo, lookup_file_info, {
	response.error_code = file_lookup_info(request->name_string_id,
	                                       request->follow_symlink,
	                                       &response.type, &response.permissions,
	                                       &response.user_id, &response.group_id,
	                                       &response.length, &response.access_time,
	                                       &response.modification_time,
	                                       &response.status_change_time);
})

FORWARD_FUNCTION(LookupSymlinkTarget, lookup_symlink_target, {
	response.error_code = symlink_lookup_target(request->name_string_id,
	                                            request->canonicalize,
	                                            &response.target_string_id);
})

//
// directory
//

FORWARD_FUNCTION(OpenDirectory, open_directory, {
	response.error_code = directory_open(request->name_string_id,
	                                     &response.directory_id);
})

FORWARD_FUNCTION(GetDirectoryName, get_directory_name, {
	response.error_code = directory_get_name(request->directory_id,
	                                         &response.name_string_id);
})

FORWARD_FUNCTION(GetNextDirectoryEntry, get_next_directory_entry, {
	response.error_code = directory_get_next_entry(request->directory_id,
	                                               &response.name_string_id,
	                                               &response.type);
})

FORWARD_FUNCTION(RewindDirectory, rewind_directory, {
	response.error_code = directory_rewind(request->directory_id);
})

FORWARD_FUNCTION(CreateDirectory, create_directory, {
	response.error_code = directory_create(request->name_string_id,
	                                       request->recursive, request->permissions,
	                                       request->user_id, request->group_id);
})

//
// process
//

FORWARD_FUNCTION(SpawnProcess, spawn_process, {
	response.error_code = process_spawn(request->executable_string_id,
	                                    request->arguments_list_id,
	                                    request->environment_list_id,
	                                    request->working_directory_string_id,
	                                    request->user_id,
	                                    request->group_id,
	                                    request->stdin_file_id,
	                                    request->stdout_file_id,
	                                    request->stderr_file_id,
	                                    &response.process_id);
})

FORWARD_FUNCTION(KillProcess, kill_process, {
	response.error_code = process_kill(request->process_id, request->signal);
})

FORWARD_FUNCTION(GetProcessCommand, get_process_command, {
	response.error_code = process_get_command(request->process_id,
	                                          &response.executable_string_id,
	                                          &response.arguments_list_id,
	                                          &response.environment_list_id,
	                                          &response.working_directory_string_id);
})

FORWARD_FUNCTION(GetProcessIdentity, get_process_identity, {
	response.error_code = process_get_identity(request->process_id,
	                                           &response.user_id,
	                                           &response.group_id);
})

FORWARD_FUNCTION(GetProcessStdio, get_process_stdio, {
	response.error_code = process_get_stdio(request->process_id,
	                                        &response.stdin_file_id,
	                                        &response.stdout_file_id,
	                                        &response.stderr_file_id);
})

FORWARD_FUNCTION(GetProcessState, get_process_state, {
	response.error_code = process_get_state(request->process_id,
	                                        &response.state,
	                                        &response.exit_code);
})

//
// program
//

FORWARD_FUNCTION(DefineProgram, define_program, {
	response.error_code = program_define(request->identifier_string_id,
	                                     &response.program_id);
})

FORWARD_FUNCTION(UndefineProgram, undefine_program, {
	response.error_code = program_undefine(request->program_id);
})

FORWARD_FUNCTION(GetProgramIdentifier, get_program_identifier, {
	response.error_code = program_get_identifier(request->program_id,
	                                             &response.identifier_string_id);
})

FORWARD_FUNCTION(GetProgramDirectory, get_program_directory, {
	response.error_code = program_get_directory(request->program_id,
	                                            &response.directory_string_id);
})

FORWARD_FUNCTION(SetProgramCommand, set_program_command, {
	response.error_code = program_set_command(request->program_id,
	                                          request->executable_string_id,
	                                          request->arguments_list_id,
	                                          request->environment_list_id);
})

FORWARD_FUNCTION(GetProgramCommand, get_program_command, {
	response.error_code = program_get_command(request->program_id,
	                                          &response.executable_string_id,
	                                          &response.arguments_list_id,
	                                          &response.environment_list_id);
})

FORWARD_FUNCTION(SetProgramStdioRedirection, set_program_stdio_redirection, {
	response.error_code = program_set_stdio_redirection(request->program_id,
	                                                    request->stdin_redirection,
	                                                    request->stdin_file_name_string_id,
	                                                    request->stdout_redirection,
	                                                    request->stdout_file_name_string_id,
	                                                    request->stderr_redirection,
	                                                    request->stderr_file_name_string_id);
})

FORWARD_FUNCTION(GetProgramStdioRedirection, get_program_stdio_redirection, {
	response.error_code = program_get_stdio_redirection(request->program_id,
	                                                    &response.stdin_redirection,
	                                                    &response.stdin_file_name_string_id,
	                                                    &response.stdout_redirection,
	                                                    &response.stdout_file_name_string_id,
	                                                    &response.stderr_redirection,
	                                                    &response.stderr_file_name_string_id);
})

FORWARD_FUNCTION(SetProgramSchedule, set_program_schedule, {
	response.error_code = program_set_schedule(request->program_id,
	                                                    request->start_condition,
	                                                    request->start_time,
	                                                    request->start_delay,
	                                                    request->repeat_mode,
	                                                    request->repeat_interval,
	                                                    request->repeat_second_mask,
	                                                    request->repeat_minute_mask,
	                                                    request->repeat_hour_mask,
	                                                    request->repeat_day_mask,
	                                                    request->repeat_month_mask,
	                                                    request->repeat_weekday_mask);
})

FORWARD_FUNCTION(GetProgramSchedule, get_program_schedule, {
	response.error_code = program_get_schedule(request->program_id,
	                                           &response.start_condition,
	                                           &response.start_time,
	                                           &response.start_delay,
	                                           &response.repeat_mode,
	                                           &response.repeat_interval,
	                                           &response.repeat_second_mask,
	                                           &response.repeat_minute_mask,
	                                           &response.repeat_hour_mask,
	                                           &response.repeat_day_mask,
	                                           &response.repeat_month_mask,
	                                           &response.repeat_weekday_mask);
})

//
// misc
//

static void api_get_identity(GetIdentityRequest *request) {
	GetIdentityResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	base58_encode(response.uid, uint32_from_le(_uid));
	strcpy(response.connected_uid, "0");
	response.position = '0';
	response.hardware_version[0] = 1; // FIXME
	response.hardware_version[1] = 0;
	response.hardware_version[2] = 0;
	response.firmware_version[0] = VERSION_MAJOR;
	response.firmware_version[1] = VERSION_MINOR;
	response.firmware_version[2] = VERSION_RELEASE;
	response.device_identifier = RED_BRICK_DEVICE_IDENTIFIER;

	network_dispatch_response((Packet *)&response);
}

#undef FORWARD_FUNCTION

//
// api
//

int api_init(void) {
	char base58[BASE58_MAX_LENGTH];

	log_debug("Initializing API subsystem");

	// read UID from /proc/red_brick_uid
	if (red_brick_uid(&_uid) < 0) {
		log_error("Could not get RED Brick UID: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	log_debug("Using %s (%u) as RED Brick UID",
	          base58_encode(base58, uint32_from_le(_uid)),
	          uint32_from_le(_uid));

	api_prepare_callback((Packet *)&_async_file_read_callback,
	                     sizeof(_async_file_read_callback),
	                     CALLBACK_ASYNC_FILE_READ);

	api_prepare_callback((Packet *)&_async_file_write_callback,
	                     sizeof(_async_file_write_callback),
	                     CALLBACK_ASYNC_FILE_WRITE);

	api_prepare_callback((Packet *)&_process_state_changed_callback,
	                     sizeof(_process_state_changed_callback),
	                     CALLBACK_PROCESS_STATE_CHANGED);

	return 0;
}

void api_exit(void) {
	log_debug("Shutting down API subsystem");
}

uint32_t api_get_uid(void) {
	return _uid;
}

void api_handle_request(Packet *request) {
	#define DISPATCH_FUNCTION(function_id_suffix, packet_prefix, function_suffix) \
		case FUNCTION_##function_id_suffix: \
			if (request->header.length != sizeof(packet_prefix##Request)) { \
				log_warn("Request has length mismatch (actual: %u != expected: %u)", \
				         request->header.length, (uint32_t)sizeof(packet_prefix##Request)); \
				api_send_response_if_expected(request, ERROR_CODE_INVALID_PARAMETER); \
			} else { \
				api_##function_suffix((packet_prefix##Request *)request); \
			} \
			break;

	switch (request->header.function_id) {
	// object
	DISPATCH_FUNCTION(RELEASE_OBJECT,                ReleaseObject,              release_object)

	// inventory
	DISPATCH_FUNCTION(OPEN_INVENTORY,                OpenInventory,              open_inventory)
	DISPATCH_FUNCTION(GET_INVENTORY_TYPE,            GetInventoryType,           get_inventory_type)
	DISPATCH_FUNCTION(GET_NEXT_INVENTORY_ENTRY,      GetNextInventoryEntry,      get_next_inventory_entry)
	DISPATCH_FUNCTION(REWIND_INVENTORY,              RewindInventory,            rewind_inventory)

	// string
	DISPATCH_FUNCTION(ALLOCATE_STRING,               AllocateString,             allocate_string)
	DISPATCH_FUNCTION(TRUNCATE_STRING,               TruncateString,             truncate_string)
	DISPATCH_FUNCTION(GET_STRING_LENGTH,             GetStringLength,            get_string_length)
	DISPATCH_FUNCTION(SET_STRING_CHUNK,              SetStringChunk,             set_string_chunk)
	DISPATCH_FUNCTION(GET_STRING_CHUNK,              GetStringChunk,             get_string_chunk)

	// list
	DISPATCH_FUNCTION(ALLOCATE_LIST,                 AllocateList,               allocate_list)
	DISPATCH_FUNCTION(GET_LIST_LENGTH,               GetListLength,              get_list_length)
	DISPATCH_FUNCTION(GET_LIST_ITEM,                 GetListItem,                get_list_item)
	DISPATCH_FUNCTION(APPEND_TO_LIST,                AppendToList,               append_to_list)
	DISPATCH_FUNCTION(REMOVE_FROM_LIST,              RemoveFromList,             remove_from_list)

	// file
	DISPATCH_FUNCTION(OPEN_FILE,                     OpenFile,                   open_file)
	DISPATCH_FUNCTION(CREATE_PIPE,                   CreatePipe,                 create_pipe)
	DISPATCH_FUNCTION(GET_FILE_INFO,                 GetFileInfo,                get_file_info)
	DISPATCH_FUNCTION(READ_FILE,                     ReadFile,                   read_file)
	DISPATCH_FUNCTION(READ_FILE_ASYNC,               ReadFileAsync,              read_file_async)
	DISPATCH_FUNCTION(ABORT_ASYNC_FILE_READ,         AbortAsyncFileRead,         abort_async_file_read)
	DISPATCH_FUNCTION(WRITE_FILE,                    WriteFile,                  write_file)
	DISPATCH_FUNCTION(WRITE_FILE_UNCHECKED,          WriteFileUnchecked,         write_file_unchecked)
	DISPATCH_FUNCTION(WRITE_FILE_ASYNC,              WriteFileAsync,             write_file_async)
	DISPATCH_FUNCTION(SET_FILE_POSITION,             SetFilePosition,            set_file_position)
	DISPATCH_FUNCTION(GET_FILE_POSITION,             GetFilePosition,            get_file_position)
	DISPATCH_FUNCTION(LOOKUP_FILE_INFO,              LookupFileInfo,             lookup_file_info)
	DISPATCH_FUNCTION(LOOKUP_SYMLINK_TARGET,         LookupSymlinkTarget,        lookup_symlink_target)

	// directory
	DISPATCH_FUNCTION(OPEN_DIRECTORY,                OpenDirectory,              open_directory)
	DISPATCH_FUNCTION(GET_DIRECTORY_NAME,            GetDirectoryName,           get_directory_name)
	DISPATCH_FUNCTION(GET_NEXT_DIRECTORY_ENTRY,      GetNextDirectoryEntry,      get_next_directory_entry)
	DISPATCH_FUNCTION(REWIND_DIRECTORY,              RewindDirectory,            rewind_directory)
	DISPATCH_FUNCTION(CREATE_DIRECTORY,              CreateDirectory,            create_directory)

	// process
	DISPATCH_FUNCTION(SPAWN_PROCESS,                 SpawnProcess,               spawn_process)
	DISPATCH_FUNCTION(KILL_PROCESS,                  KillProcess,                kill_process)
	DISPATCH_FUNCTION(GET_PROCESS_COMMAND,           GetProcessCommand,          get_process_command)
	DISPATCH_FUNCTION(GET_PROCESS_IDENTITY,          GetProcessIdentity,         get_process_identity)
	DISPATCH_FUNCTION(GET_PROCESS_STDIO,             GetProcessStdio,            get_process_stdio)
	DISPATCH_FUNCTION(GET_PROCESS_STATE,             GetProcessState,            get_process_state)

	// program
	DISPATCH_FUNCTION(DEFINE_PROGRAM,                DefineProgram,              define_program)
	DISPATCH_FUNCTION(UNDEFINE_PROGRAM,              UndefineProgram,            undefine_program)
	DISPATCH_FUNCTION(GET_PROGRAM_IDENTIFIER,        GetProgramIdentifier,       get_program_identifier)
	DISPATCH_FUNCTION(GET_PROGRAM_DIRECTORY,         GetProgramDirectory,        get_program_directory)
	DISPATCH_FUNCTION(SET_PROGRAM_COMMAND,           SetProgramCommand,          set_program_command)
	DISPATCH_FUNCTION(GET_PROGRAM_COMMAND,           GetProgramCommand,          get_program_command)
	DISPATCH_FUNCTION(SET_PROGRAM_STDIO_REDIRECTION, SetProgramStdioRedirection, set_program_stdio_redirection)
	DISPATCH_FUNCTION(GET_PROGRAM_STDIO_REDIRECTION, GetProgramStdioRedirection, get_program_stdio_redirection)
	DISPATCH_FUNCTION(SET_PROGRAM_SCHEDULE,          SetProgramSchedule,         set_program_schedule)
	DISPATCH_FUNCTION(GET_PROGRAM_SCHEDULE,          GetProgramSchedule,         get_program_schedule)

	// misc
	DISPATCH_FUNCTION(GET_IDENTITY,                  GetIdentity,                get_identity)

	default:
		log_warn("Unknown function ID %u", request->header.function_id);

		api_send_response_if_expected(request, ERROR_CODE_FUNCTION_NOT_SUPPORTED);

		break;
	}

	#undef DISPATCH_FUNCTION
}

APIE api_get_error_code_from_errno(void) {
	switch (errno) {
	case EINVAL:       return API_E_INVALID_PARAMETER;
	case ENOMEM:       return API_E_NO_FREE_MEMORY;
	case ENOSPC:       return API_E_NO_FREE_SPACE;
	case EACCES:       return API_E_ACCESS_DENIED;
	case EEXIST:       return API_E_ALREADY_EXISTS;
	case ENOENT:       return API_E_DOES_NOT_EXIST;
	case EINTR:        return API_E_INTERRUPTED;
	case EISDIR:       return API_E_IS_DIRECTORY;
	case ENOTDIR:      return API_E_NOT_A_DIRECTORY;
	case EWOULDBLOCK:  return API_E_WOULD_BLOCK;
	case EOVERFLOW:    return API_E_OVERFLOW;
	case EBADF:        return API_E_BAD_FILE_DESCRIPTOR;
	case ERANGE:       return API_E_OUT_OF_RANGE;
	case ENAMETOOLONG: return API_E_NAME_TOO_LONG;
	case ESPIPE:       return API_E_INVALID_SEEK;
	case ENOTSUP:      return API_E_NOT_SUPPORTED;

	default:           return API_E_UNKNOWN_ERROR;
	}
}

const char *api_get_function_name_from_id(int function_id) {
	switch (function_id) {
	// object
	case FUNCTION_RELEASE_OBJECT:                return "release-object";

	// inventory
	case FUNCTION_OPEN_INVENTORY:                return "open-inventory";
	case FUNCTION_GET_INVENTORY_TYPE:            return "get-inventory-type";
	case FUNCTION_GET_NEXT_INVENTORY_ENTRY:      return "get-next-inventory-entry";
	case FUNCTION_REWIND_INVENTORY:              return "rewind-inventory";

	// string
	case FUNCTION_ALLOCATE_STRING:               return "allocate-string";
	case FUNCTION_TRUNCATE_STRING:               return "truncate-string";
	case FUNCTION_GET_STRING_LENGTH:             return "get-string-length";
	case FUNCTION_SET_STRING_CHUNK:              return "set-string-chunk";
	case FUNCTION_GET_STRING_CHUNK:              return "get-string-chunk";

	// list
	case FUNCTION_ALLOCATE_LIST:                 return "allocate-list";
	case FUNCTION_GET_LIST_LENGTH:               return "get-list-length";
	case FUNCTION_GET_LIST_ITEM:                 return "get-list-item";
	case FUNCTION_APPEND_TO_LIST:                return "append-to-list";
	case FUNCTION_REMOVE_FROM_LIST:              return "remove-from-list";

	// file
	case FUNCTION_OPEN_FILE:                     return "open-file";
	case FUNCTION_CREATE_PIPE:                   return "create-pipe";
	case FUNCTION_GET_FILE_INFO:                 return "get-file-info";
	case FUNCTION_READ_FILE:                     return "read-file";
	case FUNCTION_READ_FILE_ASYNC:               return "read-file-async";
	case FUNCTION_ABORT_ASYNC_FILE_READ:         return "abort-async-file-read";
	case FUNCTION_WRITE_FILE:                    return "write-file";
	case FUNCTION_WRITE_FILE_UNCHECKED:          return "write-file-unchecked";
	case FUNCTION_WRITE_FILE_ASYNC:              return "write-file-async";
	case FUNCTION_SET_FILE_POSITION:             return "set-file-position";
	case FUNCTION_GET_FILE_POSITION:             return "get-file-position";
	case CALLBACK_ASYNC_FILE_READ:               return "async-file-read";
	case CALLBACK_ASYNC_FILE_WRITE:              return "async-file-write";
	case FUNCTION_LOOKUP_FILE_INFO:              return "lookup-file-info";
	case FUNCTION_LOOKUP_SYMLINK_TARGET:         return "lookup-symlink-target";

	// directory
	case FUNCTION_OPEN_DIRECTORY:                return "open-directory";
	case FUNCTION_GET_DIRECTORY_NAME:            return "get-directory-name";
	case FUNCTION_GET_NEXT_DIRECTORY_ENTRY:      return "get-next-directory-entry";
	case FUNCTION_REWIND_DIRECTORY:              return "rewind-directory";
	case FUNCTION_CREATE_DIRECTORY:              return "create-directory";

	// process
	case FUNCTION_SPAWN_PROCESS:                 return "spawn-process";
	case FUNCTION_KILL_PROCESS:                  return "kill-process";
	case FUNCTION_GET_PROCESS_COMMAND:           return "get-process-command";
	case FUNCTION_GET_PROCESS_IDENTITY:          return "get-process-identity";
	case FUNCTION_GET_PROCESS_STDIO:             return "get-process-stdio";
	case FUNCTION_GET_PROCESS_STATE:             return "get-process-state";
	case CALLBACK_PROCESS_STATE_CHANGED:         return "process-state-changed";

	// program
	case FUNCTION_DEFINE_PROGRAM:                return "define-program";
	case FUNCTION_UNDEFINE_PROGRAM:              return "undefine-program";
	case FUNCTION_GET_PROGRAM_IDENTIFIER:        return "get-program-identifier";
	case FUNCTION_GET_PROGRAM_DIRECTORY:         return "get-program-directory";
	case FUNCTION_SET_PROGRAM_COMMAND:           return "set-program-command";
	case FUNCTION_GET_PROGRAM_COMMAND:           return "get-program-command";
	case FUNCTION_SET_PROGRAM_STDIO_REDIRECTION: return "set-program-stdio-redirection";
	case FUNCTION_GET_PROGRAM_STDIO_REDIRECTION: return "get-program-stdio-redirection";
	case FUNCTION_SET_PROGRAM_SCHEDULE:          return "set-program-schedule";
	case FUNCTION_GET_PROGRAM_SCHEDULE:          return "get-program-schedule";

	// misc
	case FUNCTION_GET_IDENTITY:                  return "get-identity";

	default:                                     return "<unknown>";
	}
}

void api_send_async_file_read_callback(ObjectID file_id, APIE error_code,
                                       uint8_t *buffer, uint8_t length_read) {
	_async_file_read_callback.file_id = file_id;
	_async_file_read_callback.error_code = error_code;
	_async_file_read_callback.length_read = length_read;

	memcpy(_async_file_read_callback.buffer, buffer, length_read);
	memset(_async_file_read_callback.buffer + length_read, 0,
	       sizeof(_async_file_read_callback.buffer) - length_read);

	network_dispatch_response((Packet *)&_async_file_read_callback);
}

void api_send_async_file_write_callback(ObjectID file_id, APIE error_code,
                                        uint8_t length_written) {
	_async_file_write_callback.file_id = file_id;
	_async_file_write_callback.error_code = error_code;
	_async_file_write_callback.length_written = length_written;

	network_dispatch_response((Packet *)&_async_file_write_callback);
}

void api_send_process_state_changed_callback(ObjectID process_id, uint8_t state,
                                             uint8_t exit_code) {
	_process_state_changed_callback.process_id = process_id;
	_process_state_changed_callback.state = state;
	_process_state_changed_callback.exit_code = exit_code;

	network_dispatch_response((Packet *)&_process_state_changed_callback);
}

#if 0

/*
 * object
 */

+ release_object (uint16_t object_id) -> uint8_t error_code // decreases object reference count by one, frees it if reference count gets zero


/*
 * inventory
 */

enum object_type {
	OBJECT_TYPE_INVENTORY = 0,
	OBJECT_TYPE_STRING,
	OBJECT_TYPE_LIST,
	OBJECT_TYPE_FILE,
	OBJECT_TYPE_DIRECTORY,
	OBJECT_TYPE_PROCESS,
	OBJECT_TYPE_PROGRAM
}

+ open_inventory           (uint8_t type)          -> uint8_t error_code, uint16_t inventory_id
+ get_inventory_type       (uint16_t inventory_id) -> uint8_t error_code, uint8_t type
+ get_next_inventory_entry (uint16_t inventory_id) -> uint8_t error_code, uint16_t object_id // error_code == NO_MORE_DATA means end-of-inventory
+ rewind_inventory         (uint16_t inventory_id) -> uint8_t error_code


/*
 * string
 */

+ allocate_string   (uint32_t length_to_reserve)                           -> uint8_t error_code, uint16_t string_id
+ truncate_string   (uint16_t string_id, uint32_t length)                  -> uint8_t error_code
+ get_string_length (uint16_t string_id)                                   -> uint8_t error_code, uint32_t length
+ set_string_chuck  (uint16_t string_id, uint32_t offset, char buffer[58]) -> uint8_t error_code
+ get_string_chunk  (uint16_t string_id, uint32_t offset)                  -> uint8_t error_code, char buffer[63] // error_code == NO_MORE_DATA means end-of-string


/*
 * list (of objects)
 */

+ allocate_list    (uint16_t length_to_reserve)                -> uint8_t error_code, uint16_t list_id
+ get_list_length  (uint16_t list_id)                          -> uint8_t error_code, uint16_t length
+ get_list_item    (uint16_t list_id, uint16_t index)          -> uint8_t error_code, uint16_t item_object_id
+ append_to_list   (uint16_t list_id, uint16_t item_object_id) -> uint8_t error_code
+ remove_from_list (uint16_t list_id, uint16_t index)          -> uint8_t error_code


/*
 * file
 */

enum file_flag { // bitmask
	FILE_FLAG_READ_ONLY      = 0x0001,
	FILE_FLAG_WRITE_ONLY     = 0x0002,
	FILE_FLAG_READ_WRITE     = 0x0004,
	FILE_FLAG_APPEND         = 0x0008,
	FILE_FLAG_CREATE         = 0x0010,
	FILE_FLAG_EXCLUSIVE      = 0x0020,
	FILE_FLAG_NON_BLOCKING   = 0x0040,
	FILE_FLAG_TRUNCATE       = 0x0080,
	FILE_FLAG_TEMPORARY      = 0x0100 // can only be used in combination with FILE_FLAG_CREATE | FILE_FLAG_EXCLUSIVE
}

enum file_permission { // bitmask
	FILE_PERMISSION_USER_READ      = 00400,
	FILE_PERMISSION_USER_WRITE     = 00200,
	FILE_PERMISSION_USER_EXECUTE   = 00100,
	FILE_PERMISSION_GROUP_READ     = 00040,
	FILE_PERMISSION_GROUP_WRITE    = 00020,
	FILE_PERMISSION_GROUP_EXECUTE  = 00010,
	FILE_PERMISSION_OTHERS_READ    = 00004,
	FILE_PERMISSION_OTHERS_WRITE   = 00002,
	FILE_PERMISSION_OTHERS_EXECUTE = 00001
}

enum file_origin {
	FILE_ORIGIN_BEGINNING = 0,
	FILE_ORIGIN_CURRENT,
	FILE_ORIGIN_END
}

enum file_event { // bitmask
	FILE_EVENT_READ  = 0x01,
	FILE_EVENT_WRITE = 0x02
}

enum file_type {
	FILE_TYPE_UNKNOWN = 0,
	FILE_TYPE_REGULAR,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_CHARACTER,
	FILE_TYPE_BLOCK,
	FILE_TYPE_FIFO,
	FILE_TYPE_SYMLINK,
	FILE_TYPE_SOCKET,
	FILE_TYPE_PIPE
}

enum pipe_flag { // bitmask
	PIPE_FLAG_NON_BLOCKING_READ = 0x0001,
	PIPE_FLAG_NON_BLOCKING_WRITE = 0x0002
}

+ open_file             (uint16_t name_string_id, uint16_t flags, uint16_t permissions,
                         uint32_t user_id, uint32_t group_id)                           -> uint8_t error_code, uint16_t file_id
+ create_pipe           (uint16_t flags)                                                -> uint8_t error_code, uint16_t file_id
+ get_file_info         (uint16_t file_id)                                              -> uint8_t error_code, uint8_t type, uint16_t name_string_id, uint16_t flags
+ read_file             (uint16_t file_id, uint8_t length_to_read)                      -> uint8_t error_code, uint8_t buffer[62], uint8_t length_read // error_code == NO_MORE_DATA means end-of-file
+ read_file_async       (uint16_t file_id, uint64_t length_to_read)                     -> uint8_t error_code
+ abort_async_file_read (uint16_t file_id)                                              -> uint8_t error_code
+ write_file            (uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) -> uint8_t error_code, uint8_t length_written
+ write_file_unchecked  (uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) // no response
+ write_file_async      (uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) // no response
+ set_file_position     (uint16_t file_id, int64_t offset, uint8_t origin)              -> uint8_t error_code, uint64_t position
+ get_file_position     (uint16_t file_id)                                              -> uint8_t error_code, uint64_t position
? set_file_events       (uint16_t file_id, uint8_t events)                              -> uint8_t error_code
? get_file_events       (uint16_t file_id)                                              -> uint8_t error_code, uint8_t events

+ callback: async_file_read     -> uint16_t file_id, uint8_t error_code, uint8_t buffer[60], uint8_t length_read // error_code == NO_MORE_DATA means end-of-file
+ callback: async_file_write    -> uint16_t file_id, uint8_t error_code, uint8_t length_written
? callback: file_event_occurred -> uint16_t file_id, uint8_t events

+ lookup_file_info           (uint16_t name_string_id, bool follow_symlink)         -> uint8_t error_code,
                                                                                       uint8_t type,
                                                                                       uint16_t permissions,
                                                                                       uint32_t user_id,
                                                                                       uint32_t group_id,
                                                                                       uint64_t length,
                                                                                       uint64_t access_time,
                                                                                       uint64_t modification_time,
                                                                                       uint64_t status_change_time
? lookup_canonical_file_name (uint16_t name_string_id)                              -> uint8_t error_code, uint16_t canonical_name_string_id
? calculate_file_sha1_digest (uint16_t name_string_id)                              -> uint8_t error_code, uint8_t digest[20]
? remove_file                (uint16_t name_string_id, bool recursive)              -> uint8_t error_code
? rename_file                (uint16_t source_string_id, uint16_t target_string_id) -> uint8_t error_code
? create_symlink             (uint16_t target_string_id, uint16_t name_string_id)   -> uint8_t error_code
+ lookup_symlink_target      (uint16_t name_string_id, bool canonicalize)           -> uint8_t error_code, uint16_t target_string_id


/*
 * directory
 */

+ open_directory           (uint16_t name_string_id) -> uint8_t error_code, uint16_t directory_id
+ get_directory_name       (uint16_t directory_id)   -> uint8_t error_code, uint16_t name_string_id
+ get_next_directory_entry (uint16_t directory_id)   -> uint8_t error_code, uint16_t name_string_id, uint8_t type // error_code == NO_MORE_DATA means end-of-directory
+ rewind_directory         (uint16_t directory_id)   -> uint8_t error_code

+ create_directory (uint16_t name_string_id, bool recursive, uint16_t permissions, uint32_t user_id, uint32_t group_id) -> uint8_t error_code


/*
 * process
 */

enum process_signal {
	PROCESS_SIGNAL_INTERRUPT = 2,  // SIGINT
	PROCESS_SIGNAL_QUIT      = 3,  // SIGQUIT
	PROCESS_SIGNAL_ABORT     = 6,  // SIGABRT
	PROCESS_SIGNAL_KILL      = 9,  // SIGKILL
	PROCESS_SIGNAL_USER1     = 10, // SIGUSR1
	PROCESS_SIGNAL_USER2     = 12, // SIGUSR2
	PROCESS_SIGNAL_TERMINATE = 15, // SIGTERM
	PROCESS_SIGNAL_CONTINUE  = 18, // SIGCONT
	PROCESS_SIGNAL_STOP      = 19  // SIGSTOP
}

enum process_state {
	PROCESS_STATE_UNKNOWN = 0,
	PROCESS_STATE_RUNNING,
	PROCESS_STATE_EXITED, // terminated normally
	PROCESS_STATE_KILLED, // terminated by signal
	PROCESS_STATE_STOPPED // stopped by signal
}

+ spawn_process                 (uint16_t executable_string_id,
                                 uint16_t arguments_list_id,
                                 uint16_t environment_list_id,
                                 uint16_t working_directory_string_id,
                                 uint32_t user_id,
                                 uint32_t group_id,
                                 uint16_t stdin_file_id,
                                 uint16_t stdout_file_id,
                                 uint16_t stderr_file_id)             -> uint8_t error_code, uint16_t process_id
+ kill_process                  (uint16_t process_id, uint8_t signal) -> uint8_t error_code
+ get_process_command           (uint16_t process_id)                 -> uint8_t error_code,
                                                                         uint16_t executable_string_id,
                                                                         uint16_t arguments_list_id,
                                                                         uint16_t environment_list_id,
                                                                         uint16_t working_directory_string_id
+ get_process_identity          (uint16_t process_id)                 -> uint8_t error_code,
                                                                         uint32_t user_id,
                                                                         uint32_t group_id
+ get_process_stdio             (uint16_t process_id)                 -> uint8_t error_code,
                                                                         uint16_t stdin_file_id,
                                                                         uint16_t stdout_file_id,
                                                                         uint16_t stderr_file_id
+ get_process_state             (uint16_t process_id)                 -> uint8_t error_code, uint8_t state, uint8_t exit_code

+ callback: process_state_changed -> uint16_t process_id, uint8_t state, uint8_t exit_code


/*
 * (persistent) program (configuration)
 */

enum program_stdio_redirection {
	PROGRAM_STDIO_REDIRECTION_DEV_NULL = 0,
	PROGRAM_STDIO_REDIRECTION_PIPE,
	PROGRAM_STDIO_REDIRECTION_FILE
}

enum program_schedule_start_condition {
	PROGRAM_SCHEDULE_START_CONDITION_NEVER = 0,
	PROGRAM_SCHEDULE_START_CONDITION_NOW,
	PROGRAM_SCHEDULE_START_CONDITION_BOOT,
	PROGRAM_SCHEDULE_START_CONDITION_TIME
}

enum program_schedule_repeat_mode {
	PROGRAM_SCHEDULE_REPEAT_MODE_NEVER = 0,
	PROGRAM_SCHEDULE_REPEAT_MODE_RELATIVE,
	PROGRAM_SCHEDULE_REPEAT_MODE_ABSOLUTE
}

+ define_program                  (uint16_t identifier_string_id) -> uint8_t error_code, uint16_t program_id
? recover_program                 (uint16_t identifier_string_id) -> uint8_t error_code, uint16_t program_id
+ undefine_program                (uint16_t program_id)           -> uint8_t error_code
? purge_program                   (uint16_t identifier_string_id) -> uint8_t error_code
+ get_program_identifier          (uint16_t program_id)           -> uint8_t error_code, uint16_t identifier_string_id
+ get_program_directory           (uint16_t program_id)           -> uint8_t error_code, uint16_t directory_string_id
+ set_program_command             (uint16_t program_id,
                                   uint16_t executable_string_id,
                                   uint16_t arguments_list_id,
                                   uint16_t environment_list_id)  -> uint8_t error_code
+ get_program_command             (uint16_t program_id)           -> uint8_t error_code,
                                                                     uint16_t executable_string_id,
                                                                     uint16_t arguments_list_id,
                                                                     uint16_t environment_list_id
+ set_program_stdio_redirection   (uint16_t program_id,
                                   uint8_t stdin_redirection,
                                   uint16_t stdin_file_name_string_id,
                                   uint8_t stdout_redirection,
                                   uint16_t stdout_file_name_string_id,
                                   uint8_t stderr_redirection,
                                   uint16_t stderr_file_name_string_id)
                                                                  -> uint8_t error_code
+ get_program_stdio_redirection   (uint16_t program_id)           -> uint8_t error_code,
                                                                     uint8_t stdin_redirection,
                                                                     uint16_t stdin_file_name_string_id,
                                                                     uint8_t stdout_redirection,
                                                                     uint16_t stdout_file_name_string_id,
                                                                     uint8_t stderr_redirection,
                                                                     uint16_t stderr_file_name_string_id
? set_program_schedule            (uint16_t program_id,
                                   uint8_t start_condition,
                                   uint64_t start_time,
                                   uint32_t start_delay,
                                   uint8_t repeat_mode,
                                   uint32_t repeat_interval,
                                   uint64_t repeat_second_mask,
                                   uint64_t repeat_minute_mask,
                                   uint32_t repeat_hour_mask,
                                   uint32_t repeat_day_mask,
                                   uint16_t repeat_month_mask,
                                   uint8_t repeat_weekday_mask)   -> uint8_t error_code
? get_program_schedule            (uint16_t program_id)           -> uint8_t error_code
                                                                     uint8_t start_condition,
                                                                     uint64_t start_time,
                                                                     uint32_t start_delay,
                                                                     uint8_t repeat_mode,
                                                                     uint32_t repeat_interval,
                                                                     uint64_t repeat_second_mask,
                                                                     uint64_t repeat_minute_mask,
                                                                     uint32_t repeat_hour_mask,
                                                                     uint32_t repeat_day_mask,
                                                                     uint16_t repeat_month_mask,
                                                                     uint8_t repeat_weekday_mask
? get_last_program_schedule_error (uint16_t program_id)           -> uint8_t error_code, uint64_t error_time, uint16_t error_message_string_id
? get_current_program_process     (uint16_t program_id)           -> uint8_t error_code, uint16_t process_id

? callback: program_schedule_error_occurred -> uint16_t program_id, uint64_t error_time, uint16_t error_message_string_id

/*
 * misc
 */

// if the current session changed then all object IDs known to the client are invalid
? get_session () -> uint64_t session

? callback: session_changed -> uint64_t session

#endif
