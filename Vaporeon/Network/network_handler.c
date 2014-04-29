//
//  network_handler.c
//  Vaporeon
//
//  Created by Austin Zheng on 4/28/14.
//  Copyright (c) 2014 Austin Zheng. All rights reserved.
//

#include "network_handler.h"

#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/thread_status.h>

// REF:
// http://hurdextras.nongnu.org/ipc_guide/mach_ipc_basic_concepts.html
// http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/


static mach_port_name_t static_local_port = 0;
static mach_port_name_t static_remote_port = 0;

struct request_message {
    mach_msg_header_t header;
    mach_msg_size_t desc_count;
    mach_msg_ool_descriptor64_t descriptor;
    uint8_t padding[20];
};

void nh_run_handler() {
    // Do stuff here
    if (static_remote_port == 0) {
        printf("Set this up correctly, then try again.\n");
        return;
    }
    nh_get_receive_port();

    // Alloc buffer and copy in test data
    const char* msg = "This is a test message.";
    char* buffer = calloc(64, sizeof(char));
    strcpy(buffer, msg);
    
    struct request_message req_msg;
    // Forgetting the 'COMPLEX' flag caused this thing to crash with bad access errors.
    req_msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
    req_msg.header.msgh_local_port = MACH_PORT_NULL;
    req_msg.header.msgh_remote_port = static_remote_port;
    
    // Simple message, no descriptors necessary.
    // NM, need at least 1
    req_msg.desc_count = 1;
    
    // Descriptor
    req_msg.descriptor.address = (uint64_t)buffer;
    req_msg.descriptor.size = 64*sizeof(char);
    req_msg.descriptor.deallocate = FALSE;
    req_msg.descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    req_msg.descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    
    mach_msg_return_t r = mach_msg(&(req_msg.header),
                                   MACH_SEND_MSG,
                                   sizeof(req_msg),
                                   0,
                                   MACH_PORT_NULL,
                                   MACH_MSG_TIMEOUT_NONE,
                                   MACH_PORT_NULL);
    printf("Return value was 0x%x", r);
    printf("Buffer was: 0x%llx\n", (uint64_t)buffer);
    free(buffer);
}

void nh_set_remote_port(mach_port_name_t p) {
    static_remote_port = p;
}

mach_port_name_t nh_get_receive_port() {
    if (static_local_port == 0) {
        mach_port_name_t p;
        kern_return_t r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &p);
//        kern_return_t r = mach_port_allocate_full(mach_task_self(),
//                                                  MACH_PORT_RIGHT_RECEIVE,
//                                                  MACH_PORT_NULL, MACH_PORT_NULL,
//                                                  &p);
        // TODO: Handle this a little more gracefully.
        assert(r != KERN_NO_SPACE);
        static_local_port = p;
        return p;
    }
    return static_local_port;
}

