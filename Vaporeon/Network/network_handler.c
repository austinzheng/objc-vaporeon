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


static mach_port_name_t static_local_port = MACH_PORT_NULL;
static mach_port_name_t static_remote_port = MACH_PORT_NULL;

static char* static_in_buffer = NULL;
static char* static_out_buffer = NULL;

// For now, 16 MB
static uint32_t const in_buffer_size = 16777216;

struct request_message {
    mach_msg_header_t header;
    mach_msg_size_t desc_count;
    mach_msg_ool_descriptor64_t descriptor;
    uint8_t padding[20];
};

/*!
 Perform whatever cleanup is necessary to precede a clean server shutdown.
 */
void cleanup() {
    if (static_in_buffer != NULL) {
        free(static_in_buffer);
    }
    if (static_out_buffer != NULL) {
        free(static_out_buffer);
    }
}

/*!
 Given a size in bytes, round up to the nearest multiple of 64.
 */
uint32_t nearest_64(uint32_t size) {
    if (size < 64) {
        return 64;
    }
    else {
        return size + (size % 64);
    }
}

/*!
 Dispatch a message to the \c objc-vaporeon server instance.
 */
boolean_t dispatch_to_server(const char* buffer, uint32_t length, uint32_t* error) {
    const uint32_t adjusted_length = nearest_64(length);
    if (!static_in_buffer) {
        // Alloc buffer, since it's the first time
        static_in_buffer = calloc(in_buffer_size, sizeof(char));
        if (!static_in_buffer) goto DTS_ALLOC_FAIL;
    }
    else {
        // Zero out buffer
        memset(static_in_buffer, 0, in_buffer_size*sizeof(char));
    }
    if (static_remote_port == MACH_PORT_NULL) {
        // The runloop listener hasn't registered with the module yet, there is nowhere to send the data
        return FALSE;
    }
    else if (adjusted_length > in_buffer_size) {
        // TODO: Can we split messages up here, or should that go elsewhere?
        return FALSE;
    }
    memcpy(static_in_buffer, buffer, adjusted_length);
    
    struct request_message req_msg;
    // Header
    req_msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
    req_msg.header.msgh_local_port = MACH_PORT_NULL;
    req_msg.header.msgh_remote_port = static_remote_port;
    
    // Descriptor
    req_msg.desc_count = 1;
    req_msg.descriptor.address = (uint64_t)buffer;
    req_msg.descriptor.size = adjusted_length;
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
    
    if (r == MACH_MSG_SUCCESS) {
        return TRUE;
    }
    else if (error != NULL) {
        *error = r;
        return FALSE;
    }
    
DTS_ALLOC_FAIL:
    // Need to exit program, since there is no point in continuing if we can't even allocate the buffer.
    return FALSE;
}

// Eventually, rewrite this
void nh_run_handler() {
    // Do stuff here
    if (static_remote_port == MACH_PORT_NULL) {
        printf("Set this up correctly, then try again.\n");
        return;
    }
    const char* msg = "This is a test message.";
    dispatch_to_server(msg, sizeof(msg), NULL);
}

void nh_set_remote_port(mach_port_name_t p) {
    static_remote_port = p;
}

mach_port_name_t nh_get_receive_port() {
    if (static_local_port == MACH_PORT_NULL) {
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

