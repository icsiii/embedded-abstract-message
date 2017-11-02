// Copyright (C) 2017 Verizon, Inc. All rights reserved.
#ifndef TS_MESSAGE_H
#define TS_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "cJSON.h"

#include "ts_common.h"

// static memory model (warning - affects bss directly)
// in the spirit of the static-memory design pattern established for the ts-sdk,
// we allow you to avoid malloc and free, instead, we can preallocate memory for
// all of the concurrent message nodes we could ever require. this can easily be
// switched over to a dynamic memory model by commenting out this define.
//#define TS_MESSAGE_STATIC_MEMORY

// maximum number of roots
// this is just for guidance - at runtime, the application could allocate
// all of the nodes for just one message (however, note TS_MESSAGE_MAX_DEPTH).
#define TS_MESSAGE_MAX_ROOTS        3

// maximum number of branches allowed per node
// for TsTypeMessage, limits the number of attributes per JSON/CBOR object
// for TsTypeArray, limits the maximum size of the array.
#define TS_MESSAGE_MAX_BRANCHES     15

// total number of nodes available for messages
#define TS_MESSAGE_MAX_NODES    ( TS_MESSAGE_MAX_BRANCHES * TS_MESSAGE_MAX_ROOTS )

// maximum depth of a message
// note, this value doesnt affect bss
#define TS_MESSAGE_MAX_DEPTH        5

// maximum size of a string attribute
// i.e., length of a uuid with dashes (36) plus termination
#define TS_MESSAGE_MAX_STRING_SIZE  37

// maximum size of a key (i.e., field name)
#define TS_MESSAGE_MAX_KEY_SIZE     24

// supported encoders
typedef enum {
    TsEncoderDebug,
    TsEncoderJson,
    TsEncoderCbor,
} TsEncoder_t;

// field path node
typedef char* TsPathNode_t;

// field path
typedef TsPathNode_t* TsPath_t;

// supported encoded field types
typedef enum {
    TsTypeInteger,      // int*
    TsTypeFloat,        // float*
    TsTypeBoolean,      // stdbool, bool*
    TsTypeString,       // zero terminated byte array (i.e., char *)
    TsTypeMessage,      // TsMessage_t*[N], where N is the number of fields
    TsTypeArray,        // TsMessage_t*[N], where N is the number of elements
    TsTypeNull          // no value
} TsType_t;

// forward reference and typedef to TsMessage pointer
typedef struct TsMessage * TsMessageRef_t;

// value
typedef void* TsValue_t;

// field value
// note, union size will take the largest attribute
typedef union TsField * TsFieldRef_t;
typedef union {
    int             _xinteger;
    float           _xfloat;
    bool            _xboolean;
    char            _xstring[ TS_MESSAGE_MAX_STRING_SIZE ];
    TsMessageRef_t  _xfields[ TS_MESSAGE_MAX_BRANCHES ];
    TsFieldRef_t    _xitems[ TS_MESSAGE_MAX_BRANCHES ];
} TsField_t;

// a single message node binding
// (which, during runtime, could be either a root or a branch node)
typedef struct TsMessage {
    int             references;
    char            name[ TS_MESSAGE_MAX_KEY_SIZE ];
    TsType_t        type;
    TsField_t       value;
} TsMessage_t;

#ifdef __cplusplus
extern "C" {
#endif

TsStatus_t ts_message_create( TsMessageRef_t * message );
TsStatus_t ts_message_create_array( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value );
TsStatus_t ts_message_create_message( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value );
TsStatus_t ts_message_destroy( TsMessageRef_t message );

TsStatus_t ts_message_set( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t value );
TsStatus_t ts_message_set_null( TsMessageRef_t message, TsPathNode_t field );
TsStatus_t ts_message_set_int( TsMessageRef_t message, TsPathNode_t field, int value );
TsStatus_t ts_message_set_float( TsMessageRef_t message, TsPathNode_t field, float value );
TsStatus_t ts_message_set_string( TsMessageRef_t message, TsPathNode_t field, char * value );
TsStatus_t ts_message_set_bool( TsMessageRef_t message, TsPathNode_t field, bool value );
TsStatus_t ts_message_set_array( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t value );
TsStatus_t ts_message_set_message( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t value );

TsStatus_t ts_message_has( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value );

TsStatus_t ts_message_get( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value );
TsStatus_t ts_message_get_int( TsMessageRef_t message, TsPathNode_t field, int * value );
TsStatus_t ts_message_get_float( TsMessageRef_t message, TsPathNode_t field, float * value );
TsStatus_t ts_message_get_string( TsMessageRef_t message, TsPathNode_t field, char ** value );
TsStatus_t ts_message_get_bool( TsMessageRef_t message, TsPathNode_t field, bool * value );
TsStatus_t ts_message_get_array( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value );
TsStatus_t ts_message_get_message( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value );

// TODO
TsStatus_t ts_message_get_size( TsMessageRef_t message, size_t* size);
TsStatus_t ts_message_get_index( TsMessageRef_t message, size_t index, TsType_t* type, TsValue_t* value );

TsStatus_t ts_message_encode( TsMessageRef_t message, TsEncoder_t encoder, uint8_t * buffer, size_t * buffer_size );
TsStatus_t ts_message_decode( TsMessageRef_t message, TsEncoder_t encoder, uint8_t * buffer, size_t buffer_size );
TsStatus_t ts_message_decode_json( TsMessageRef_t message, int depth, cJSON * value );

#ifdef __cplusplus
}
#endif

#endif // TS_MESSAGE_H
