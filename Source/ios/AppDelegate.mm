#if defined (_WIN32) || defined (_WIN64)
  #define       JUCE_WINDOWS 1
#elif defined (JUCE_ANDROID)
  #undef        JUCE_ANDROID
  #define       JUCE_ANDROID 1
#elif defined (__FreeBSD__) || (__OpenBSD__)
  #define       JUCE_BSD 1
#elif defined (LINUX) || defined (__linux__)
  #define       JUCE_LINUX 1
#elif defined (__APPLE_CPP__) || defined (__APPLE_CC__)
  #define CF_EXCLUDE_CSTD_HEADERS 1
  #include <TargetConditionals.h> // (needed to find out what platform we're using)
  #include <AvailabilityMacros.h>

  #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
    #define     JUCE_IPHONE 1
    #define     JUCE_IOS 1
  #else
    #define     JUCE_MAC 1
  #endif
#elif defined (__wasm__)
  #define       JUCE_WASM 1
#else
  #error "Unknown platform!"
#endif

#if JUCE_IOS

#import "AppDelegate.h"

#import <React/RCTBridge.h>
#import <React/RCTBundleURLProvider.h>
#import <React/RCTRootView.h>

#ifdef FB_SONARKIT_ENABLED
#import <FlipperKit/FlipperClient.h>
#import <FlipperKitLayoutPlugin/FlipperKitLayoutPlugin.h>
#import <FlipperKitUserDefaultsPlugin/FKUserDefaultsPlugin.h>
#import <FlipperKitNetworkPlugin/FlipperKitNetworkPlugin.h>
#import <SKIOSNetworkPlugin/SKIOSNetworkAdapter.h>
#import <FlipperKitReactPlugin/FlipperKitReactPlugin.h>

static void InitializeFlipper(UIApplication *application) {
  FlipperClient *client = [FlipperClient sharedClient];
  SKDescriptorMapper *layoutDescriptorMapper = [[SKDescriptorMapper alloc] initWithDefaults];
  [client addPlugin:[[FlipperKitLayoutPlugin alloc] initWithRootNode:application withDescriptorMapper:layoutDescriptorMapper]];
  [client addPlugin:[[FKUserDefaultsPlugin alloc] initWithSuiteName:nil]];
  [client addPlugin:[FlipperKitReactPlugin new]];
  [client addPlugin:[[FlipperKitNetworkPlugin alloc] initWithNetworkAdapter:[SKIOSNetworkAdapter new]]];
  [client start];
}
#endif

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
#ifdef FB_SONARKIT_ENABLED
  InitializeFlipper(application);
#endif

    [juceDelegate applicationDidFinishLaunching:application];

    RCTBridge *bridge = [[RCTBridge alloc] initWithDelegate:self launchOptions:launchOptions];
    RCTRootView *rootView = [[RCTRootView alloc] initWithBridge:bridge
                                                   moduleName:@"RNJuce"
                                            initialProperties:nil];

    if (@available(iOS 13.0, *)) {
        rootView.backgroundColor = [UIColor systemBackgroundColor];
    } else {
        rootView.backgroundColor = [UIColor whiteColor];
    }

    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    UIViewController *rootViewController = [UIViewController new];
    rootViewController.view = rootView;
    self.window.rootViewController = rootViewController;
    [self.window makeKeyAndVisible];
    return YES;
}

- (NSURL *)sourceURLForBridge:(RCTBridge *)bridge
{
#if DEBUG
  return [[RCTBundleURLProvider sharedSettings] jsBundleURLForBundleRoot:@"index" fallbackResource:nil];
#else
  return [[NSBundle mainBundle] URLForResource:@"main" withExtension:@"jsbundle"];
#endif
}

-(id) init
{
    self = [super init];
    juceDelegate = reinterpret_cast<NSObject<UIApplicationDelegate>*> ([[NSClassFromString (@"JuceAppStartupDelegate") alloc] init]);
    return self;
}

-(void) dealloc
{
    [juceDelegate release];
    [super dealloc];
}

- (void) forwardInvocation: (NSInvocation*) anInvocation
{
    if (juceDelegate != nullptr && [juceDelegate respondsToSelector: [anInvocation selector]])
        [anInvocation invokeWithTarget: juceDelegate];
    else
        [super forwardInvocation: anInvocation];
}

-(BOOL) respondsToSelector: (SEL) aSelector
{
    if (juceDelegate != nullptr && [juceDelegate respondsToSelector: aSelector])
        return YES;

    return [super respondsToSelector: aSelector];
}

@end

#endif
