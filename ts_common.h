/* Copyright (C) 2017 Verizon, Inc. All rights reserved. */
#ifndef TS_COMMON_H
#define TS_COMMON_H

/* common status responses */
typedef enum {
	TsStatusUnknown = 0,                    /* status unknown */
	TsStatusOkEnqueue = 100,                /* partial success, subsequent operation is expected (e.g., trying) */
	TsStatusOk = 200,                       /* operation complete success */
	TsStatusError = 400,                    /* a generic error and the start of the status error section */
	TsStatusErrorBadRequest = 400,          /* failed due to wrong parameters values (same value as a generic error) */
	TsStatusErrorNotFound = 404,            /* failed due to missing information */
	TsStatusErrorPreconditionFailed = 412,  /* failed due to missing or wrong data or parameters */
	TsStatusErrorPayloadTooLarge = 413,     /* failed due to large data set(s) */
	TsStatusErrorRecursionTooDeep = 414,    /* failed due to stack limitations */
	TsStatusErrorInternalServerError = 500, /* failed due to an unknown critical error (e.g., seg-fault) */
	TsStatusErrorNotImplemented = 501,      /* operation not ready for use */
	TsStatusErrorOutOfMemory = 600,         /* failed due to soft (detectable) out-of-memory state */
	TsStatusErrorIndexOutOfRange = 601		/* index is out of range */
} TsStatus_t;

#endif /* TS_COMMON_H */
