#pragma once

#include <pebble.h>

#define STRAP_BUFFER_SIZE 256

void strap_init();


//void strap_request_data();
void strap_request_data(char *buf) ;
