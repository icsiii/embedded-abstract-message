// Copyright (C) 2017 Verizon, Inc. All rights reserved.

#include <string.h>
#include <stdio.h>
#include "cbor.h"
#include "cJSON.h"

// client debug
// dbg_printf()
#include "dbg.h"

#include "ts_common.h"
#include "ts_message.h"

// static memory model (warning - affects bss directly)
// in the spirit of the static-memory design pattern established for the ts-sdk,
// we allow you to avoid malloc and free, instead, we can preallocate memory for
// all of the concurrent message nodes we could ever require. this can easily be
// switched over to a dynamic memory model by commenting out the
// TS_MESSAGE_STATIC_MEMORY define.
#ifdef TS_MESSAGE_STATIC_MEMORY
static TsMessage_t _ts_message_nodes[TS_MESSAGE_MAX_NODES];
static bool _ts_message_nodes_initialized = false;
#endif

// forward references
#ifdef TS_MESSAGE_STATIC_MEMORY
static TsStatus_t _ts_message_initialize();
#endif
static TsStatus_t _ts_message_set( TsMessageRef_t, TsPathNode_t, TsType_t, TsValue_t );
static TsStatus_t _ts_message_encode_debug( TsMessageRef_t, int );
static TsStatus_t _ts_message_encode_json( TsMessageRef_t, int, uint8_t *, size_t );
static TsStatus_t _ts_message_encode_cbor( TsMessageRef_t, int, CborEncoder *, uint8_t *, size_t );

// ts_message_create
TsStatus_t ts_message_create( TsMessageRef_t * message ) {

#ifdef TS_MESSAGE_STATIC_MEMORY
    // initialize static memory system
    if( !_ts_message_nodes_initialized ) {
        _ts_message_initialize();
    }

    // search for next free
    for( int i = 0; i < TS_MESSAGE_MAX_NODES; i++ ) {

        if( _ts_message_nodes[ i ].references == 0 ) {

            // mark as assigned
            _ts_message_nodes[ i ].references = 1;

            // clear all, assume root (avoiding memset)
            snprintf( _ts_message_nodes[ i ].name, TS_MESSAGE_MAX_KEY_SIZE, "$root" );
            _ts_message_nodes[ i ].type = TsTypeMessage;
            for( int j = 0; j < TS_MESSAGE_MAX_BRANCHES; j++ ) {
                _ts_message_nodes[ i ].value._xfields[ j ] = NULL;
            }

            // set the return value (root)
            *message = &_ts_message_nodes[ i ];

            // return ok
            return TsStatusOk;
        }
    }

    // if none found, then clear the return value
    *message = NULL;

    // and return an out-of-memory error
    return TsStatusErrorOutOfMemory;

#else

    *message = (TsMessageRef_t)(malloc( sizeof( TsMessage_t ) ));

    memset( *message, 0x00, sizeof( TsMessage_t ) );
    (*message)->references = 1;
    (*message)->type = TsTypeMessage;
    snprintf( (*message)->name, TS_MESSAGE_MAX_KEY_SIZE, "$root" );

    return TsStatusOk;
#endif
}

// ts_message_create_message
TsStatus_t ts_message_create_message( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value ) {

    // allocate a single message node
    TsStatus_t status = ts_message_create( value );
    if( status == TsStatusOk ) {

        // set the field relative to the given message to the new message
        snprintf( ( *value )->name, TS_MESSAGE_MAX_KEY_SIZE, "%s", field );
        ( *value )->type = TsTypeMessage;
        status = _ts_message_set( message, field, TsTypeMessage, *value );
    }

    // always attempt clean up,...
    // i.e., if the set was successful, the reference counter should be (at least) 2 at this point,
    // in which case the message wont be free'd. This is the same behavior that the caller would
    // have performed if they had created the message and called set themselves.
    ts_message_destroy( *value );

    // return result
    return status;
}

// ts_message_create_array
TsStatus_t ts_message_create_array( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value ) {

    // allocate a single message node
    TsStatus_t status = ts_message_create( value );
    if( status == TsStatusOk ) {

        // set the field relative to the given message to the new message
        snprintf( ( *value )->name, TS_MESSAGE_MAX_KEY_SIZE, "%s", field );
        ( *value )->type = TsTypeArray;
        status = _ts_message_set( message, field, TsTypeArray, *value );
    }

    // always attempt clean up,...
    // i.e., if the set was successful, the reference counter should be (at least) 2 at this point,
    // in which case the message wont be free'd. This is the same behavior that the caller would
    // have performed if they had created the message and called set themselves.
    ts_message_destroy( *value );

    // return result
    return status;
}

// ts_message_destroy
TsStatus_t ts_message_destroy( TsMessageRef_t message ) {

    // check preconditions
    if( message == NULL || message->references <= 0 ) {
        return TsStatusErrorPreconditionFailed;
    }

    // simply change its status
    message->references--;

    // and destroy along with children
    if( message->references <= 0 ) {

        if( message->type == TsTypeArray || message->type == TsTypeMessage ) {
            for( int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {
                if( message->value._xfields[ i ] != NULL) {
                    ts_message_destroy( message->value._xfields[ i ] );
                }
            }
        }
#ifdef TS_MESSAGE_STATIC_MEMORY
        message->references = 0;
#else
        free( message );
#endif
    }

    // return ok
    return TsStatusOk;
}

// ts_message_set
TsStatus_t ts_message_set( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t value ) {

    // hold the type of the value, since set will force it to be TsTypeMessage
    TsType_t type = value->type;

    // find the best field (e.g., by name) and set that field to this value
    TsStatus_t status = _ts_message_set( message, field, TsTypeMessage, value );

    // reset the "new" field (i.e., the given pointer with references bumped by one)
    // to the correct type.
    value->type = type;

    return status;
}

// ts_message_set_null
TsStatus_t ts_message_set_null( TsMessageRef_t message, TsPathNode_t field ) {
    return _ts_message_set( message, field, TsTypeNull, NULL);
}

// ts_message_set_int
TsStatus_t ts_message_set_int( TsMessageRef_t message, TsPathNode_t field, int value ) {
    return _ts_message_set( message, field, TsTypeInteger, &value );
}

// ts_message_set_float
TsStatus_t ts_message_set_float( TsMessageRef_t message, TsPathNode_t field, float value ) {
    return _ts_message_set( message, field, TsTypeFloat, &value );
}

// ts_message_set_string
TsStatus_t ts_message_set_string( TsMessageRef_t message, TsPathNode_t field, char * value ) {
    return _ts_message_set( message, field, TsTypeString, value );
}

// ts_message_set_bool
TsStatus_t ts_message_set_bool( TsMessageRef_t message, TsPathNode_t field, bool value ) {
    return _ts_message_set( message, field, TsTypeBoolean, &value );
}

// ts_message_set_array
TsStatus_t ts_message_set_array( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t value ) {
    return _ts_message_set( message, field, TsTypeArray, value );
}

// ts_message_set_message
TsStatus_t ts_message_set_message( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t value ) {
    return _ts_message_set( message, field, TsTypeMessage, value );
}

// ts_message_has
TsStatus_t ts_message_has( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value ) {
    if( message == NULL || field == NULL || value == NULL ) {
        return TsStatusErrorPreconditionFailed;
    }
    if( message->type != TsTypeMessage ) {
        return TsStatusErrorPreconditionFailed;
    }
    for( int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {
        TsMessageRef_t object = message->value._xfields[ i ];
        if( object == NULL) {
            return TsStatusErrorNotFound;
        }
        if( strcmp( object->name, field ) == 0 ) {
            *value = object;
            return TsStatusOk;
        }
    }
    return TsStatusErrorNotFound;
}

// ts_message_get
TsStatus_t ts_message_get( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value ) {
    return ts_message_has( message, field, value );
}

// ts_message_get_int
TsStatus_t ts_message_get_int( TsMessageRef_t message, TsPathNode_t field, int * value ) {
    TsMessageRef_t object;
    if( ts_message_has( message, field, &object ) == TsStatusOk ) {
        if( object->type == TsTypeInteger ) {
            *value = object->value._xinteger;
            return TsStatusOk;
        } else {
            return TsStatusErrorPreconditionFailed;
        }
    }
    return TsStatusErrorNotFound;
}

// ts_message_get_float
TsStatus_t ts_message_get_float( TsMessageRef_t message, TsPathNode_t field, float * value ) {
    TsMessageRef_t object;
    if( ts_message_has( message, field, &object ) == TsStatusOk ) {
        if( object->type == TsTypeFloat ) {
            *value = object->value._xfloat;
            return TsStatusOk;
        } else {
            return TsStatusErrorPreconditionFailed;
        }
    }
    return TsStatusErrorNotFound;
}

// ts_message_get_string
TsStatus_t ts_message_get_string( TsMessageRef_t message, TsPathNode_t field, char ** value ) {
    TsMessageRef_t object;
    if( ts_message_has( message, field, &object ) == TsStatusOk ) {
        if( object->type == TsTypeString ) {
            *value = object->value._xstring;
            return TsStatusOk;
        } else {
            return TsStatusErrorPreconditionFailed;
        }
    }
    return TsStatusErrorNotFound;
}

// ts_message_get_bool
TsStatus_t ts_message_get_bool( TsMessageRef_t message, TsPathNode_t field, bool * value ) {
    TsMessageRef_t object;
    if( ts_message_has( message, field, &object ) == TsStatusOk ) {
        if( object->type == TsTypeBoolean ) {
            *value = object->value._xboolean;
            return TsStatusOk;
        } else {
            return TsStatusErrorPreconditionFailed;
        }
    }
    return TsStatusErrorNotFound;
}

// ts_message_get_array
TsStatus_t ts_message_get_array( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value ) {
    TsMessageRef_t object;
    if( ts_message_has( message, field, &object ) == TsStatusOk ) {
        if( object->type == TsTypeArray ) {
            *value = object;
            return TsStatusOk;
        } else {
            return TsStatusErrorPreconditionFailed;
        }
    }
    return TsStatusErrorNotFound;
}

// ts_message_get_message
TsStatus_t ts_message_get_message( TsMessageRef_t message, TsPathNode_t field, TsMessageRef_t * value ) {
    TsMessageRef_t object;
    if( ts_message_has( message, field, &object ) == TsStatusOk ) {
        if( object->type == TsTypeMessage ) {
            *value = object;
            return TsStatusOk;
        } else {
            return TsStatusErrorPreconditionFailed;
        }
    }
    return TsStatusErrorNotFound;
}

// ts_message_get_size
TsStatus_t ts_message_get_size( TsMessageRef_t message, size_t * size ) {
    // not implemented
    return TsStatusErrorNotImplemented;
}

// ts_message_get_index
TsStatus_t ts_message_get_index( TsMessageRef_t message, size_t index, TsType_t * type, TsValue_t * value ) {
    // not implemented
    return TsStatusErrorNotImplemented;
}

// ts_message_encode
// encode will attempt to fill the given buffer with the encoded data found in the given message.
TsStatus_t ts_message_encode( TsMessageRef_t message, TsEncoder_t encoder, uint8_t * buffer, size_t * buffer_size ) {

    // check preconditions
    if( message == NULL) {
        return TsStatusErrorPreconditionFailed;
    }

    // perform encoding
    switch( encoder ) {
        case TsEncoderDebug:

            return _ts_message_encode_debug( message, 0 );

        case TsEncoderJson: {

            if( buffer == NULL) {
                return TsStatusErrorBadRequest;
            }
            memset( buffer, 0x00, *buffer_size );
            TsStatus_t status = _ts_message_encode_json( message, 0, buffer, *buffer_size );
            *buffer_size = strlen((char *) buffer );
            return status;
        }

        case TsEncoderCbor: {

            if( buffer == NULL) {
                return TsStatusErrorBadRequest;
            }
            CborEncoder cbor;
            cbor_encoder_init( &cbor, buffer, *buffer_size, 0 );
            TsStatus_t status = _ts_message_encode_cbor( message, 0, &cbor, buffer, *buffer_size );
            *buffer_size = cbor_encoder_get_buffer_size( &cbor, buffer );
            return status;
        }

        default:
            // do nothing
            break;
    }
    return TsStatusErrorNotImplemented;
}

// ts_message_set
TsStatus_t ts_message_decode( TsMessageRef_t message, TsEncoder_t encoder, uint8_t * buffer, size_t buffer_size ) {

    // check preconditions
    if( message == NULL) {
        return TsStatusErrorPreconditionFailed;
    }

    // perform encoding
    switch( encoder ) {

        case TsEncoderJson: {

            if( buffer == NULL) {
                return TsStatusErrorBadRequest;
            }

            cJSON * cjson = cJSON_Parse( (const char *)buffer );
            if( cjson->type == cJSON_Object ) {
                cjson = cjson->child;
            }
            TsStatus_t status = ts_message_decode_json( message, 0, cjson );
            cJSON_Delete( cjson );

            return status;
        }

        case TsEncoderCbor:
            // not implemented
            // fallthrough

        case TsEncoderDebug:
        default:
            // do nothing
            break;
    }
    return TsStatusErrorNotImplemented;
}

// ts_message_decode_json
TsStatus_t ts_message_decode_json( TsMessageRef_t message, int depth, cJSON * value ) {

    // decode each node in the current value
    TsStatus_t status = TsStatusOk;
    while( value != NULL) {

        // decode current node
        switch( value->type ) {

            case cJSON_NULL:
                status = ts_message_set_null( message, value->string );
                break;

            case cJSON_Number:
                if( value->valuedouble == (double) ( value->valueint )) {
                    status = ts_message_set_int( message, value->string, value->valueint );
                } else {
                    status = ts_message_set_float( message, value->string, (float) ( value->valuedouble ));
                }
                break;

            case cJSON_String:
                status = ts_message_set_string( message, value->string, value->valuestring );
                break;

            case cJSON_True:
                status = ts_message_set_bool( message, value->string, true );
                break;

            case cJSON_False:
                status = ts_message_set_bool( message, value->string, false );
                break;

            case cJSON_Object: {

                TsMessageRef_t content;
                status = ts_message_create_message( message, value->string, &content );
                if( status == TsStatusOk ) {
                    status = ts_message_decode_json( content, depth + 1, value->child );
                }
                break;
            }
            case cJSON_Array:
                // TODO
                // fallthrough

            case cJSON_Invalid:
            case cJSON_Raw:
            default:
                return TsStatusErrorNotImplemented;
        }

        // get next sibling
        value = value->next;
    }
    return status;
}

// ////////////////////////////////////////////////////////////////////////////
// P R I V A T E

#ifdef TS_MESSAGE_STATIC_MEMORY
// (private) _ts_message_initialize
static TsStatus_t _ts_message_initialize() {

    // report some basic statistics
    dbg_printf( "initializing messaging, message_t size (%lu) preallocated message nodes (%d)\n", sizeof( TsMessage_t ),
                TS_MESSAGE_MAX_NODES);

    // initialize message management system
    for( int i = 0; i < TS_MESSAGE_MAX_NODES; i++ ) {

        // just mark everything free
        _ts_message_nodes[ i ].references = 0;
    }
    _ts_message_nodes_initialized = true;

    // return ok
    return TsStatusOk;
}
#endif

/**
 * Set the current message node to the given type and value. The optional field may be used to set a node relative
 * to the one given, e.g., as in a JSON object field.
 * @param message
 * The object of the action, set.
 * @param field
 * The optional field name, e.g., a JSON object field. If this value is NULL, then the given message node type and
 * value is set with the ones given. Otherwise, if this value isn't NULL, then the given message node is treated as
 * an object, and the value as the fields, where one field is named, typed and valued with the values provided.
 * @param type
 * The type of the message, e.g., TsTypeInteger, TsTypeFloat, etc.
 * @param value
 * The value of the message, e.g., int, float, etc.
 * @return
 * The status of the call as defined by ts_common.h
 */
static TsStatus_t _ts_message_set( TsMessageRef_t message, TsPathNode_t field, TsType_t type, TsValue_t value ) {

    // check preconditions
    if( message == NULL || ( type != TsTypeNull && value == NULL)) {
        return TsStatusErrorPreconditionFailed;
    }

    // search for the relevant node
    // normally assume we're adding or modifying a field,
    // however we will check for primitives during the first iteration
    for( int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {

        // check for primitives
        TsMessageRef_t branch = message;
        if( field != NULL) {

            // the path node is either new or has been established previously
            branch = message->value._xfields[ i ];
            if( branch == NULL || strcmp( field, branch->name ) == 0 ) {

                // establish branch
                switch( type ) {

                    case TsTypeInteger:
                    case TsTypeFloat:
                    case TsTypeBoolean:
                    case TsTypeString:
                    case TsTypeNull:

                        // destroy the old message if overwriting a new value
                        if( branch != NULL) {
                            ts_message_destroy( branch );
                        }

                        // (re)create a new messsage
                        TsStatus_t status = ts_message_create( &branch );
                        if( status != TsStatusOk ) {
                            return status;
                        }
                        break;

                    case TsTypeMessage:
                    case TsTypeArray:

                        // clear out old node and establish new
                        ts_message_destroy( branch );
                        branch = (TsMessageRef_t) value;

                        // the caller is expected to destroy the given message (i.e., held by 'value'),
                        // by bumping the reference we insure the message doesnt free.
                        branch->references++;
                        break;
                }
            } else {
                continue;
            }

            // (re)set this field array to the updated branch
            message->value._xfields[ i ] = branch;
        }

        // (re)set the field name and type
        snprintf( branch->name, TS_MESSAGE_MAX_KEY_SIZE, "%s", field );
        branch->type = type;

        // (re)set the field value
        switch( type ) {

            case TsTypeInteger:

                branch->value._xinteger = *((int *) ( value ));
                break;

            case TsTypeFloat:

                branch->value._xfloat = *((float *) ( value ));
                break;

            case TsTypeBoolean:

                branch->value._xboolean = *((bool *) ( value ));
                break;

            case TsTypeString:

                snprintf( branch->value._xstring, TS_MESSAGE_MAX_STRING_SIZE, "%s", (char *) value );
                if( strlen( branch->value._xstring ) < strlen((char *) value )) {
                    dbg_printf( "issue detected during set (%s), string truncated; the given string is too large\n",
                                field );
                }
                break;

            case TsTypeMessage:
            case TsTypeArray:
            case TsTypeNull:

                // do nothing
                break;
        }

        return TsStatusOk;
    }

    // there isn't a branch available
    dbg_printf( "failed to set (%s), there are no additional nodes available\n", field );
    return TsStatusErrorPayloadTooLarge;
}

// _ts_message_encode_none
// simple debug based 'encoder', display the structure of the message as it stands
static TsStatus_t _ts_message_encode_debug( TsMessageRef_t message, int depth ) {

    // insure we limit message depth
    // (for the sake of limiting stack size)
    if( depth > TS_MESSAGE_MAX_DEPTH ) {
        return TsStatusErrorRecursionTooDeep;
    }

    // pretty print (indent)
    for( int i = 0; i < depth; i++ ) {
        dbg_printf( "  " );
    }
    dbg_printf( "%s", message->name );

    // display type and value
    switch( message->type ) {
        case TsTypeNull:
            dbg_printf( ":NULL\n" );
            break;

        case TsTypeInteger:
            dbg_printf( ":integer( %d )\n", message->value._xinteger );
            break;

        case TsTypeFloat:
            dbg_printf( ":float( %f )\n", message->value._xfloat );
            break;

        case TsTypeBoolean:
            dbg_printf( ":boolean( %u )\n", message->value._xboolean );
            break;

        case TsTypeString:
            dbg_printf( ":string( %s )\n", message->value._xstring );
            break;

        case TsTypeArray: {
            dbg_printf( ":array\n" );
            for( int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {
                dbg_printf( "[%d] = {\n", i );
                TsMessageRef_t branch = message->value._xfields[ i ];
                if( branch == NULL) {
                    break;
                } else {
                    _ts_message_encode_debug( branch, depth + 1 );
                }
                dbg_printf( "}\n" );
            }
        }
        case TsTypeMessage: {
            dbg_printf( ":message\n" );
            for( int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {
                TsMessageRef_t branch = message->value._xfields[ i ];
                if( branch == NULL) {
                    break;
                } else {
                    _ts_message_encode_debug( branch, depth + 1 );
                }
            }
            break;
        }
        default:
            dbg_printf( ":unknown\n" );
            break;
    }
    return TsStatusOk;
}

// _ts_message_encode_json
static TsStatus_t _ts_message_encode_json( TsMessageRef_t message, int depth, uint8_t * buffer, size_t buffer_size ) {

    // insure we limit message depth
    // (for the sake of limiting stack size)
    if( depth > TS_MESSAGE_MAX_DEPTH ) {
        return TsStatusErrorRecursionTooDeep;
    }

    // re-point buffer append
    // TODO - check for negative sizes, etc.
    char * xbuffer = (char *) buffer;
    size_t xbuffer_size = buffer_size - strlen( xbuffer );
    xbuffer = xbuffer + strlen( xbuffer );

    // display type and value
    switch( message->type ) {
        case TsTypeNull:
            snprintf( xbuffer, xbuffer_size, "null" );
            break;

        case TsTypeInteger:
            snprintf( xbuffer, xbuffer_size, "%d", message->value._xinteger );
            break;

        case TsTypeFloat:
            snprintf( xbuffer, xbuffer_size, "%f", message->value._xfloat );
            break;

        case TsTypeBoolean:
            snprintf( xbuffer, xbuffer_size, "%s", message->value._xboolean ? "true" : "false" );
            break;

        case TsTypeString:
            snprintf( xbuffer, xbuffer_size, "\"%s\"", message->value._xstring );
            break;

        case TsTypeArray: {
            // TODO - add array
            return TsStatusErrorNotImplemented;
        }
        case TsTypeMessage: {
            snprintf( xbuffer + strlen( xbuffer ), xbuffer_size - strlen( xbuffer ), "{" );
            for( int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {
                TsMessageRef_t branch = message->value._xfields[ i ];
                if( branch == NULL) {
                    break;
                } else {
                    // TODO - check for negative sizes, etc.
                    if( i > 0 ) {
                        snprintf( xbuffer + strlen( xbuffer ), xbuffer_size - strlen( xbuffer ), "," );
                    }
                    snprintf( xbuffer + strlen( xbuffer ), xbuffer_size - strlen( xbuffer ), "\"%s\":", branch->name );
                    _ts_message_encode_json( branch, depth + 1, buffer, buffer_size );
                }
            }
            snprintf( xbuffer + strlen( xbuffer ), xbuffer_size - strlen( xbuffer ), "}" );
            break;
        }
        default:
            return TsStatusErrorInternalServerError;
    }

    // check if we've used up the buffer
    // note, snprintf is safe, it wont overwrite the given buffer size
    // note also, the buffer_size includes the terminating null
    if( strlen((char *) buffer ) >= ( buffer_size - 1 )) {
        return TsStatusErrorOutOfMemory;
    }
    return TsStatusOk;
}

// _ts_message_encode_cbor
static TsStatus_t _ts_message_encode_cbor( TsMessageRef_t message, int depth, CborEncoder * encoder, uint8_t * buffer,
                                           size_t buffer_size ) {

    // insure we limit message depth
    // (for the sake of limiting stack size)
    if( depth > TS_MESSAGE_MAX_DEPTH ) {
        return TsStatusErrorRecursionTooDeep;
    }

    // display type and value
    switch( message->type ) {
        case TsTypeNull:
            cbor_encode_text_stringz( encoder, message->name );
            cbor_encode_null( encoder );
            break;

        case TsTypeInteger:
            cbor_encode_text_stringz( encoder, message->name );
            cbor_encode_int( encoder, message->value._xinteger );
            break;

        case TsTypeFloat:
            cbor_encode_text_stringz( encoder, message->name );
            cbor_encode_float( encoder, message->value._xfloat );
            break;

        case TsTypeBoolean:
            cbor_encode_text_stringz( encoder, message->name );
            cbor_encode_boolean( encoder, message->value._xboolean );
            break;

        case TsTypeString:
            cbor_encode_text_stringz( encoder, message->name );
            cbor_encode_text_stringz( encoder, message->value._xstring );
            break;

        case TsTypeArray: {
            // TODO - add array
            return TsStatusErrorNotImplemented;
        }
        case TsTypeMessage: {

            // tag if not root
            if( strcmp( message->name, "$root" ) != 0 ) {
                cbor_encode_text_stringz( encoder, message->name );
            }

            // determine size of map
            // TODO - avoid search, hold current free index in root
            size_t length = TS_MESSAGE_MAX_BRANCHES;
            for( size_t i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {
                if( message->value._xfields[ i ] == NULL) {
                    length = i;
                    break;
                }
            }

            // create and fill map
            CborEncoder map;
            cbor_encoder_create_map( encoder, &map, length );
            for( int i = 0; i < length; i++ ) {
                _ts_message_encode_cbor( message->value._xfields[ i ], depth + 1, &map, buffer, buffer_size );
            }
            cbor_encoder_close_container( encoder, &map );
            break;
        }
        default:
            return TsStatusErrorInternalServerError;
    }

    // check if we've used up the buffer
    // TODO - need to double check to see if cbor-encode functions can overrun buffer (dont think they do tho)
    if( cbor_encoder_get_buffer_size( encoder, buffer ) >= buffer_size ) {
        return TsStatusErrorOutOfMemory;
    }
    return TsStatusOk;
}
