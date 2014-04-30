//
//  main.m
//  Vaporeon
//
//  Created by Austin Zheng on 4/28/14.
//  Copyright (c) 2014 Austin Zheng. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "VPONetworkManager.h"

#import "network_handler.h"
#import <mach/mach.h>
#import <mach/mach_port.h>

void perform_run();

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"Server starting...");
        
        VPONetworkManager *manager = [VPONetworkManager sharedInstance];
        nh_set_remote_port(manager.portName);
//        nh_get_receive_port();
        
        dispatch_queue_t network_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), network_queue, ^{
            NSLog(@"Starting network manager...");
            start(2130706433, 8003);
            // Perform the run-loop test
//            perform_run();
        });
        
        [[NSRunLoop currentRunLoop] run];
    }
    return 0;
}

void perform_run() {
    nh_run_handler();
    dispatch_queue_t network_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)), network_queue, ^{
        perform_run();
    });
}
