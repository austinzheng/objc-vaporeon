//
//  network_handler.h
//  Vaporeon
//
//  Created by Austin Zheng on 4/28/14.
//  Copyright (c) 2014 Austin Zheng. All rights reserved.
//

#ifndef Vaporeon_network_handler_h
#define Vaporeon_network_handler_h

#include <mach/mach_types.h>

mach_port_name_t nh_get_receive_port();
void nh_set_remote_port(mach_port_name_t p);

// TODO: remove this
void nh_run_handler();

#endif
