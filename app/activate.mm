#include "activate.hpp"

#include <cstdio>

#import <Foundation/Foundation.h>
#import <SystemExtensions/SystemExtensions.h>

#include <CoreFoundation/CoreFoundation.h>

namespace {

NSString* const kExtensionId = @"dev.dsmobile620.driver";

}  // namespace

@interface DS620ActivationDelegate : NSObject <OSSystemExtensionRequestDelegate>
@property(nonatomic) BOOL done;
@property(nonatomic) BOOL ok;
@property(nonatomic) BOOL reboot;
@property(nonatomic, copy) NSString* message;
@property(nonatomic) CFRunLoopRef runLoop;
@end

@implementation DS620ActivationDelegate

- (OSSystemExtensionReplacementAction)request:(OSSystemExtensionRequest*)request
                 actionForReplacingExtension:(OSSystemExtensionProperties*)existing
                               withExtension:(OSSystemExtensionProperties*)ext {
    (void)request;
    (void)existing;
    (void)ext;
    return OSSystemExtensionReplacementActionReplace;
}

- (void)requestNeedsUserApproval:(OSSystemExtensionRequest*)request {
    (void)request;
    std::fprintf(stderr,
                 "Approve %s in the system dialog. This command waits until you do.\n",
                 kExtensionId.UTF8String);
}

- (void)request:(OSSystemExtensionRequest*)request didFinishWithResult:(OSSystemExtensionRequestResult)result {
    (void)request;
    self.ok = YES;
    self.reboot = result == OSSystemExtensionRequestWillCompleteAfterReboot;
    self.done = YES;
    CFRunLoopStop(self.runLoop);
}

- (void)request:(OSSystemExtensionRequest*)request didFailWithError:(NSError*)error {
    (void)request;
    self.ok = NO;
    self.message = error.localizedDescription;
    self.done = YES;
    CFRunLoopStop(self.runLoop);
}

@end

namespace ds620 {

Activation activateDriver(std::string& message) {
    @autoreleasepool {
        DS620ActivationDelegate* delegate = [DS620ActivationDelegate new];
        delegate.runLoop = CFRunLoopGetCurrent();
        OSSystemExtensionRequest* request =
            [OSSystemExtensionRequest activationRequestForExtension:kExtensionId queue:dispatch_get_main_queue()];
        request.delegate = delegate;
        [[OSSystemExtensionManager sharedManager] submitRequest:request];
        while (!delegate.done) {
            CFRunLoopRun();
        }
        if (delegate.message != nil) {
            message = delegate.message.UTF8String;
        }
        if (!delegate.ok) {
            return Activation::Failed;
        }
        if (delegate.reboot) {
            return Activation::NeedsReboot;
        }
        return Activation::Completed;
    }
}

}  // namespace ds620
