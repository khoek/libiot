#pragma once

#include "private.h"

void libiot_init_sntp();

// This function blocks until the network time has been synced for the first
// time.
void libiot_start_sntp();
