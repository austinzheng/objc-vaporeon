//
//  VPONetworkManager.h
//  Vaporeon
//
//  Created by Austin Zheng on 4/28/14.
//  Copyright (c) 2014 Austin Zheng. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface VPONetworkManager : NSObject

@property (nonatomic, readonly) mach_port_name_t portName;

+ (instancetype)sharedInstance;

@end
