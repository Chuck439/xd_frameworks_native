/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_UI_GRALLOC2_H
#define ANDROID_UI_GRALLOC2_H

#include <string>

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>
#include <android/hardware/graphics/common/1.1/types.h>
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <android/hardware/graphics/mapper/2.1/IMapper.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <utils/StrongPointer.h>

namespace android {

namespace Gralloc2 {

using hardware::graphics::allocator::V2_0::IAllocator;
using hardware::graphics::mapper::V2_1::IMapper;

// A wrapper to IMapper
class Mapper {
public:
    static void preload();

    Mapper();

    status_t createDescriptor(void* bufferDescriptorInfo, void* outBufferDescriptor) const;

    // Import a buffer that is from another HAL, another process, or is
    // cloned.
    //
    // The returned handle must be freed with freeBuffer.
    status_t importBuffer(const hardware::hidl_handle& rawHandle,
                          buffer_handle_t* outBufferHandle) const;

    void freeBuffer(buffer_handle_t bufferHandle) const;

    status_t validateBufferSize(buffer_handle_t bufferHandle, uint32_t width, uint32_t height,
                                android::PixelFormat format, uint32_t layerCount, uint64_t usage,
                                uint32_t stride) const;

    void getTransportSize(buffer_handle_t bufferHandle,
            uint32_t* outNumFds, uint32_t* outNumInts) const;

    // The ownership of acquireFence is always transferred to the callee, even
    // on errors.
    status_t lock(buffer_handle_t bufferHandle, uint64_t usage, const Rect& bounds,
                  int acquireFence, void** outData) const;

    // The ownership of acquireFence is always transferred to the callee, even
    // on errors.
    status_t lock(buffer_handle_t bufferHandle, uint64_t usage, const Rect& bounds,
                  int acquireFence, android_ycbcr* ycbcr) const;

    // unlock returns a fence sync object (or -1) and the fence sync object is
    // owned by the caller
    int unlock(buffer_handle_t bufferHandle) const;

private:
    // Determines whether the passed info is compatible with the mapper.
    status_t validateBufferDescriptorInfo(IMapper::BufferDescriptorInfo* descriptorInfo) const;

    sp<hardware::graphics::mapper::V2_0::IMapper> mMapper;
    sp<IMapper> mMapperV2_1;
};

// A wrapper to IAllocator
class Allocator {
public:
    // An allocator relies on a mapper, and that mapper must be alive at all
    // time.
    Allocator(const Mapper& mapper);

    std::string dumpDebugInfo() const;

    /*
     * The returned buffers are already imported and must not be imported
     * again.  outBufferHandles must point to a space that can contain at
     * least "bufferCount" buffer_handle_t.
     */
    status_t allocate(uint32_t width, uint32_t height, PixelFormat format, uint32_t layerCount,
                      uint64_t usage, uint32_t bufferCount, uint32_t* outStride,
                      buffer_handle_t* outBufferHandles) const;

private:
    const Mapper& mMapper;
    sp<IAllocator> mAllocator;
};

} // namespace Gralloc2

} // namespace android

#endif // ANDROID_UI_GRALLOC2_H
