#ifndef WESTEROS_SINK_L1_BACKEND_MOCK_H
#define WESTEROS_SINK_L1_BACKEND_MOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int initCalls, termCalls, acceptCapsCalls, renderCalls, flushCalls;
    int eosCalls, startVideoCalls, setStartPtsCalls, queryCalls, stateTransitionCalls;
    int64_t lastStartPts;
    bool acceptCaps, handleQueries;
} SinkBackendMockState;

void sink_backend_mock_reset(void);
SinkBackendMockState sink_backend_mock_state(void);
void sink_backend_mock_set_accept_caps(bool accept);

void sink_backend_mock_set_handle_queries(bool handle);
#endif