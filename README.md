objc-vaporeon
=============

objc-vaporeon, the bubble jet web server.

About
-----

objc-vaporeon is (will be) a toy web framework for OS X written in C and Objective-C. I have several objectives in mind:

- Experiment with and learn about low-level programming topics such as POSIX/Berkeley sockets, Mach ports/IPC, etc.
- See if Objective-C characteristics and features (such as native compilation, ARC, Core Data, blocks, GCD) can be used to create a performant web server.
- Emulate node.js's asynchronous event-driven behavior using the Cocoa runloop abstraction.

Given the lack of an Objective-C ANSI/ISO/IEEE standard, its anemic adoption outside iOS/OS X client development (besides GNUstep), and the cost of hosting Apple hardware, I don't expect this to be anything more than an academic exercise/curiosity.

Running
-------

Download the Xcode project. In ``main.c``, change the second parameter to a port of your choice. Then run the project and hit localhost:<port> with a web browser.

Right now, the project does only a few things:

- Accept a TCP connection.
- Use a Mach port to send the HTTP request to the runloop context.
- Send back a basic HTTP reply.

License
-------

Copyright © 2014 Austin Zheng. Released under the terms of the MIT License.
