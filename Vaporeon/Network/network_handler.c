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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "assert.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/thread_status.h>

// REF:
// http://hurdextras.nongnu.org/ipc_guide/mach_ipc_basic_concepts.html
// http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/

static int const S_BACKLOG = 128;

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

#pragma mark - Forward declarations

void run(int descriptor);
int prepare_sockets(in_addr_t addr, in_port_t port);
boolean_t dispatch_to_server(const char* buffer, uint32_t length, uint32_t* error);

#pragma mark - Lifecycle

/*!
 Start the network module
 */
void start(in_addr_t addr, in_port_t port) {
    // Should try addr = 2130706433 (= 127.0.0.1), port = 3001.
    int descriptor = prepare_sockets(addr, port);
    if (descriptor < 0) {
        return;
    }
    if (0 != listen(descriptor, S_BACKLOG)) {
        printf("Error: unable to begin listening on socket. ERRNO is: %d\n", errno);
    }
    run(descriptor);
}

void run(int descriptor) {
    struct sockaddr_in remote_addr;
    socklen_t remote_addr_len = 0;
    int sock_fd = -1;
    
    // Set up receive buffer
    // TODO: make this not completely broken
    int const SIZE = 2097152;
    char* recv_buffer = calloc(SIZE, sizeof(char));
    if (!recv_buffer) {
        printf("Error: unable to allocate receive buffer");
        goto RUN_FAIL;
    }
    char* send_buffer = calloc(SIZE, sizeof(char));
    if (!send_buffer) {
        printf("Error: unable to allocate send buffer");
        goto RUN_FAIL;
    }
    
    while (1) {
        // This only supports one connection. Testing code.
        remote_addr_len = sizeof(struct sockaddr_in);
        if (sock_fd == -1) {
            // If we don't have a connection, wait for one.
            sock_fd = accept(descriptor, (struct sockaddr *)&remote_addr, &remote_addr_len);
            if (sock_fd == -1) {
                printf("Error: accept() failed. ERRNO is: %d\n", errno);
                goto RUN_FAIL;
            }
            // Make socket nonblocking
            // TODO: I turned this off for testing, since it makes closing the socket after the reply is sent nontrivial.
//            if (-1 == fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL) | O_NONBLOCK)) {
//                printf("Error: unable to make socket nonblocking. ERRNO is: %d\n", errno);
//                goto RUN_FAIL;
//            }
        }
        
        // Receive data
        memset(recv_buffer, 0, SIZE*sizeof(char));
        ssize_t recv_length = recvfrom(sock_fd, recv_buffer, (size_t)SIZE*sizeof(char), 0, NULL, NULL);
        if (recv_length == -1) {
            if (errno != EAGAIN) {
                // Something went wrong, and it wasn't the nonblocking socket not having data
                printf("Error: recvfrom() failed. ERRNO is: %d\n", errno);
                goto RUN_FAIL;
            }
            // Continue, nothing to read
        }
        else {
            // Got data, process it
            // For now, just send it to the runloop
            printf("Got data. Received %ld bytes.\n", recv_length);
            dispatch_to_server(recv_buffer, (uint32_t)recv_length, NULL);
            
            // Send a canned reply
            char* reply = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: 51\r\nConnection: close\r\n\r\n<html><head></head><body>Hello world</body></html>\r\n";
//            printf(reply);
            unsigned long reply_len = strlen(reply);
            strncpy(send_buffer, reply, reply_len);
            ssize_t send_len = send(sock_fd, send_buffer, reply_len, 0);
            if (send_len == -1) {
                printf("Error: send() failed. ERRNO is: %d\n", errno);
                goto RUN_FAIL;
            }
            else if (send_len != reply_len) {
                printf("Error: send() returned unexpected result. Expecting %ld, got %ld. ERRNO is: %d\n",
                       sizeof(reply), send_len, errno);
                goto RUN_FAIL;
            }
            // For now, close the connection
            printf("Closing socket...\n");
            close(sock_fd);
            sock_fd = -1;
        }
    }
    return;
RUN_FAIL:
    // Cleanup
    if (sock_fd != -1) {
        close(sock_fd);
    }
    return;
}



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
    else if (size > UINT32_MAX - 64) {
        return UINT32_MAX;
    }
    else {
        return size + (size % 64);
    }
}


#pragma mark - Sockets

int prepare_sockets(in_addr_t addr, in_port_t port) {
    // Try to create and open up a socket
    const int TCP_PROTOCOL = 6;
    int descriptor = socket(PF_INET, SOCK_STREAM, TCP_PROTOCOL);
    if (descriptor < 0) {
        printf("Error: unable to get socket. Returned descriptor was %d\n", descriptor);
        goto PS_FAIL;
    }

    struct sockaddr_in saddr;
    saddr.sin_len = sizeof(in_addr_t);
    saddr.sin_family = AF_INET;
    // Convert addresses to network order.
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(addr);
    if (0 != bind(descriptor, (struct sockaddr*)(&saddr), sizeof(saddr))) {
        printf("Error: unable to bind socket. ERRNO is: %d\n", errno);
        goto PS_FAIL;
    }
    return descriptor;

PS_FAIL:
    // Cleanup can go here
    return -1;
}


#pragma mark - Mach ports

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


#pragma mark - Other stuff

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

