#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include "dbg.h"
#include "cbor.h"
#include "ts_message.h"

// example struct to encode
typedef struct {
	bool zwitch;
	char comment[256];
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
static TsStatus_t test04();
static TsStatus_t test05();
static TsStatus_t test06();

#define CC_MAX_SEND_BUF_SZ 2048

// main
int main()
{

	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = mysighandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGSEGV, &sigIntHandler, NULL);

	TsStatus_t status = test06();
	if (status != TsStatusOk) {
		printf("an error occurred while encoding, %d\n", status);
	}
	exit(0);
}

// mysighandler
static void mysighandler()
{
	printf("\nfault\n");
	exit(0);
}

static TsStatus_t test06()
{
	// TODO - MEMORY LEAK SHOWS UP WHEN USING THIS CODE...

	TsMessageRef_t sensor, location;
	ts_message_create(&sensor);
	ts_message_set_float(sensor, "temperature", 57.7);
	ts_message_create_message(sensor, "location", &location);
	ts_message_set_float(location, "latitude", 42.361145f);
	ts_message_set_float(location, "longitude", -71.057083f);

	/* create message content */
	TsMessageRef_t message, sensors, characteristics;
	ts_message_create(&message);
	ts_message_set_string(message, "unitName", "unit-name");
	ts_message_set_string(message, "unitMacId", "device-id");
	ts_message_set_string(message, "unitSerialNo", "unit-serial-number");
	ts_message_create_message(message, "sensor", &sensors);
	ts_message_create_array(sensors, "characteristics", &characteristics);

	/* for each field of the message,... */
	for (int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++ ) {

		/* the "for-each" behavior terminates on a NULL or max-size */
		TsMessageRef_t branch = sensor->value._xfields[ i ];
		if (branch == NULL) {
			break;
		}

		/* transform into the form expected by the server */
		TsMessageRef_t characteristic;
		ts_message_create(&characteristic);
		ts_message_set_string(characteristic, "characteristicsName", branch->name);
		ts_message_set(characteristic, "currentValue", branch);
		ts_message_set_message_at(characteristics, i, characteristic);
		ts_message_encode(characteristic, TsEncoderDebug, NULL, 0);
		ts_message_destroy(characteristic);
	}

	/* encode copy to send buffer */
	uint8_t buffer[2048];
	size_t buffer_size = 2048;
	ts_message_encode(message, TsEncoderJson, buffer, &buffer_size);

	/* TODO - remove debug */
	ts_message_encode(message, TsEncoderDebug, NULL, 0);

	/* clean up */
	ts_message_destroy(message);
	ts_message_destroy(sensor);
	ts_message_report();

	return TsStatusOk;
}

static TsStatus_t test05()
{

	TsMessageRef_t message, sensors;
	ts_message_create(&message);

	ts_message_create_array(message, "sensors", &sensors);
	ts_message_set_float_at(sensors, 0, 45.0f);
	ts_message_set_float_at(sensors, 1, 46.5f);
	ts_message_set_float_at(sensors, 2, 56.0f);
	ts_message_encode(message, TsEncoderDebug, NULL, NULL);

	ts_message_destroy(message);

	return TsStatusOk;
}

static TsStatus_t test04()
{

	// test create
	TsMessageRef_t sensor;
	ts_message_create(&sensor);
	ts_message_set_int(sensor, "setting", 12);
	ts_message_set_float(sensor, "temperature", 52.2);

	TsMessageRef_t message_foo;
	ts_message_create_message(sensor, "foo", &message_foo);
	ts_message_set_bool(message_foo, "switch", true);
	ts_message_set_string(message_foo, "comment", "this is my comment");

	// create message with header
	char content[CC_MAX_SEND_BUF_SZ];
	char content_format[CC_MAX_SEND_BUF_SZ] = "%s{\"characteristicsName\":\"%s\",\"currentValue\":%s}";

	memset(content, 0x00, CC_MAX_SEND_BUF_SZ);
	for (int i = 0; i < TS_MESSAGE_MAX_BRANCHES; i++) {

		TsMessageRef_t branch = sensor->value._xfields[i];
		if (branch == NULL) {
			break;
		}

		char value[CC_MAX_SEND_BUF_SZ];
		size_t value_size = CC_MAX_SEND_BUF_SZ;
		ts_message_encode(branch, TsEncoderJson, (uint8_t *) value, &value_size);

		snprintf(content + strlen(content), CC_MAX_SEND_BUF_SZ - strlen(content), content_format,
				 i > 0 ? "," : "",
				 branch->name,
				 value);
	}

	char message[CC_MAX_SEND_BUF_SZ];
	char message_format[CC_MAX_SEND_BUF_SZ] =
		"{\"unitName\":\"%s\",\"unitMacId\":\"%s\",\"unitSerialNo\":\"%s\",\"sensor\":{\"characteristics\":[%s]}}";
	snprintf(message, CC_MAX_SEND_BUF_SZ, message_format,
			 "unit-name",
			 "unit-device-id",
			 "unit-serial-number",
			 content);

	printf("message = (%s)\n", message);
	return TsStatusOk;
}
static TsStatus_t test03()
{

	// test create
	TsMessageRef_t message, sensors, myself;
	TsStatus_t status = ts_message_create(&message);
	if (status != TsStatusOk) {
		return status;
	}

	//ts_message_set_int( message, NULL, 5 );

	// test set and get
	ts_message_set_int(message, "setting", 12);
	ts_message_set_float(message, "temperature", 52.2);

	TsMessageRef_t message_foo;
	ts_message_create_message(message, "foo", &message_foo);
	//ts_message_create_message( message, "foo", &message_foo );

	ts_message_set_bool(message_foo, "switch", true);
	//ts_message_set_bool( message_foo, "switch", false );
	ts_message_set_string(message_foo, "comment", "this is my comment");

	ts_message_create_array(message, "sensors", &sensors);
	ts_message_set_float_at(sensors, 0, 45.0f);
	ts_message_set_float_at(sensors, 1, 46.5f);
	ts_message_set_float_at(sensors, 2, 56.0f);

	ts_message_create_array(message, "myself", &myself);
	ts_message_set_message_at(myself, 0, message);
	ts_message_set_message_at(myself, 1, message);
	ts_message_set_message_at(myself, 2, message);

	ts_message_encode(message, TsEncoderDebug, NULL, NULL);


	// test encoding
	TsEncoder_t encoder = TsEncoderJson;
	uint8_t buffer[2048];
	size_t buffer_size = sizeof(buffer);
	status = ts_message_encode(message, encoder, buffer, &buffer_size);
	if (status != TsStatusOk) {
		return status;
	}
	switch (encoder) {
	case TsEncoderCbor:
		printf("\n");
		for (int i = 0; i < buffer_size; i++) {
			printf("%02x ", buffer[i]);
		}
		printf("\n(length = %zu)\n", buffer_size);
		printf("\n");
		break;
	case TsEncoderJson:
		printf("%s\n(length = %zu)\n", buffer, buffer_size);
		break;
	case TsEncoderDebug:
	default:
		// do nothing
		break;
	}

	// test destroy
	status = ts_message_destroy(message);
	if (status != TsStatusOk) {
		return status;
	}


	// test decoding
	if (encoder == TsEncoderJson) {

		status = ts_message_create(&message);
		if (status != TsStatusOk) {
			return status;
		}
		status = ts_message_decode(message, encoder, buffer, buffer_size);
		if (status != TsStatusOk) {
			return status;
		}
		status = ts_message_encode(message, TsEncoderDebug, buffer, &buffer_size);
		if (status != TsStatusOk) {
			return status;
		}
		status = ts_message_destroy(message);
		if (status != TsStatusOk) {
			return status;
		}
	}



	// return ok
	return TsStatusOk;
}
