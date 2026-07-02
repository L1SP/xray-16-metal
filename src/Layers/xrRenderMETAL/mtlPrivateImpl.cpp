// This file provides the implementation of metal-cpp private symbols.
// It must be compiled in exactly one translation unit, without the PCH,
// because NS/MTL/CA_PRIVATE_IMPLEMENTATION must be defined before
// any metal-cpp headers are included.
//
// This file is excluded from the Unity Build to avoid duplicate symbols.

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
