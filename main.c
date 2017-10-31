#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include "cbor.h"
#include "ts_message.h"

// example struct to encode
typedef struct {
    bool zwitch;
    char comment[ 256 ];
} Foo_t;

typedef struct {
    int setting;
    float temperature;
    Foo_t foo;
} Goo_t;

// forward references
static void mysighandler();
static TsStatus_t test01();
static TsStatus_t test03();

// main
int main() {

    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = mysighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGSEGV, &sigIntHandler, NULL);


    TsStatus_t status = test03();
    if( status != TsStatusOk ) {
        printf( "an error occurred while encoding, %d\n", status );
    }
    exit(0);
}


// mysighandler
static void mysighandler() {
    printf("\nfault\n");
    exit(0);
}

static TsStatus_t test03() {

    // test create
    TsMessageRef_t message;
    TsStatus_t status = ts_message_create( &message );
    if( status != TsStatusOk ) {
        return status;
    }

    //ts_message_set_int( message, NULL, 5 );

    // test set and get
    ts_message_set_int( message, "setting", 12 );
    ts_message_set_float( message, "temperature", 52.2 );

    TsMessageRef_t message_foo;
    ts_message_create_message( message, "foo", &message_foo );
    //ts_message_create_message( message, "foo", &message_foo );

    ts_message_set_bool( message_foo, "switch", true );
    //ts_message_set_bool( message_foo, "switch", false );
    ts_message_set_string( message_foo, "comment", "this is my comment" );

    // test encoding
    TsEncoder_t encoder = TsEncoderJson;
    uint8_t buffer[ 256 ];
    size_t buffer_size = sizeof( buffer );
    status = ts_message_encode( message, encoder, buffer, &buffer_size );
    if( status != TsStatusOk ) {
        return status;
    }
    switch( encoder ) {
        case TsEncoderCbor:
            printf("\n");
            for (int i = 0; i < buffer_size; i++) {
                printf("%02x ", buffer[i]);
            }
            printf("\n(length = %zu)\n",buffer_size);
            printf("\n");
            break;
        case TsEncoderJson:
            printf("%s\n(length = %zu)\n",buffer,buffer_size);
            break;
        case TsEncoderDebug:
        default:
            // do nothing
            break;
    }

    // test destroy
    status = ts_message_destroy( message );
    if( status != TsStatusOk ) {
        return status;
    }


    // test decoding
    if( encoder == TsEncoderJson ) {

        status = ts_message_create( &message );
        if( status != TsStatusOk ) {
            return status;
        }
        status = ts_message_decode( message, encoder, buffer, buffer_size );
        if( status != TsStatusOk ) {
            return status;
        }
        status = ts_message_encode( message, TsEncoderDebug, buffer, &buffer_size );
        if( status != TsStatusOk ) {
            return status;
        }
        status = ts_message_destroy( message );
        if( status != TsStatusOk ) {
            return status;
        }
    }



    // return ok
    return TsStatusOk;
}
