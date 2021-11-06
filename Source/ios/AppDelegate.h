#import <React/RCTBridgeDelegate.h>
#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate, RCTBridgeDelegate> {
    NSObject<UIApplicationDelegate>* juceDelegate;
}

@property (nonatomic, strong) UIWindow *window;

@end
