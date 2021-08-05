#pragma once

#include "private.h"

void libiot_init_sntp();

// This function blocks until the network time has been received.
void libiot_start_sntp();
