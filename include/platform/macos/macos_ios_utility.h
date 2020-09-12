#pragma once

#include <string>

#import <MetalKit/MetalKit.h>

namespace iris::platform::utility
{

/**
 * Helper function to convert a std::string to a NSString.
 *
 * @param str
 *   String to convert.
 *
 * @returns
 *   Supplied string converted to NSString.
 */
NSString* string_to_nsstring(const std::string &str);

/**
 * Get the metal device object for the current view.
 *
 * @returns
 *   Metal device.
 */
id<MTLDevice> metal_device();

/**
 * Get the metal layer for the current view.
 *
 * @returns
 *   Metal layer.
 */
CAMetalLayer* metal_layer();

/**
 * Get the natural scale for the screen. This value reflects the scale factor
 * needed to convert from the default logical coordinate space into the device
 * coordinate space of this screen.
 *
 * @returns
 *   Screen scale factor.
 */
CGFloat screen_scale();

}


