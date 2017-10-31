// Copyright (C) 2017 Verizon, Inc. All rights reserved.
#ifndef TS_COMMON_H
#define TS_COMMON_H

// message channels
typedef enum {
    TsChannelRequest,   // Request channel for command (set, get, getotp) JSON encoded data
    TsChannelResponse,  // Response channel for command (set, get, getotp response) JSON encoded data
    TsChannelSensor,    // Response channel for sensor (unsolicited get) JSON encoded data
    TsChannelDiagnostic // Request and Response channel for diagnostic (getotp) CBOR encoded data
} TsChannel_t;

// status response from event
typedef enum {
    TsStatusUnknown                 = 0,
    TsStatusOkEnqueue               = 100,
    TsStatusOk                      = 200,
    TsStatusError                   = 400,
    TsStatusErrorBadRequest         = 400, // Error and BadRequest are the same
    TsStatusErrorNotFound           = 404,
    TsStatusErrorPreconditionFailed = 412,
    TsStatusErrorPayloadTooLarge    = 413,
    TsStatusErrorRecursionTooDeep   = 414,
    TsStatusErrorInternalServerError = 500,
    TsStatusErrorNotImplemented     = 501,
    TsStatusErrorOutOfMemory        = 502,
} TsStatus_t;

#endif //TS_COMMON_H
