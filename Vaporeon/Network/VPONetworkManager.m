//
//  VPONetworkManager.m
//  Vaporeon
//
//  Created by Austin Zheng on 4/28/14.
//  Copyright (c) 2014 Austin Zheng. All rights reserved.
//

#import "VPONetworkManager.h"

#import <mach/mach.h>
#import <mach/mach_port.h>


@interface VPONetworkManager () <NSPortDelegate>
@property (nonatomic, strong) NSPort *port;
@property (nonatomic, readwrite) mach_port_name_t portName;
@end

@implementation VPONetworkManager

+ (instancetype)sharedInstance {
    static VPONetworkManager *static_instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        static_instance = [[self class] new];
        [static_instance setupPort];
    });
    return static_instance;
}

- (void)setupPort {
    mach_port_name_t p = MACH_PORT_NULL;
    ipc_space_t task = mach_task_self();
    kern_return_t r = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &p);
//    kern_return_t r = mach_port_allocate_full(task,
//                                              MACH_PORT_RIGHT_RECEIVE,
//                                              MACH_PORT_NULL,
//                                              MACH_PORT_NULL,
//                                              &p);
    assert(r != KERN_NO_SPACE);
    self.portName = p;
    NSPort *port = [NSMachPort portWithMachPort:p];
    port.delegate = self;
    [port scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    self.port = port;
}


#pragma mark - NSPortDelegate

- (void)handlePortMessage:(NSPortMessage *)message {
    NSData *data = message.components[0];
    NSString *str = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
    NSLog(@"Received message with data: %@", str);
}

@end
