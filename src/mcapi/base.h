#pragma once

typedef struct NBT NBT;

typedef enum mcapiConnState { MCAPI_STATE_INIT = 0,
                              MCAPI_STATE_STATUS = 1,
                              MCAPI_STATE_LOGIN = 2,
                              MCAPI_STATE_CONFIG = 3,
                              MCAPI_STATE_PLAY = 4 } mcapiConnState;

typedef struct mcapiConnection mcapiConnection;

mcapiConnection* mcapi_create_connection(char* hostname, short port, char* uuid, char* access_token);
void mcapi_destroy_connection(mcapiConnection* conn);

void mcapi_set_state(mcapiConnection* conn, mcapiConnState state);
mcapiConnState mcapi_get_state(mcapiConnection* conn);

void mcapi_poll(mcapiConnection* conn);
