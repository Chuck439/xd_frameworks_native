#ifndef ANDROID_DVR_BUFFERHUB_RPC_H_
#define ANDROID_DVR_BUFFERHUB_RPC_H_

#include <cutils/native_handle.h>
#include <gui/BufferQueueDefs.h>
#include <sys/types.h>

#include <pdx/channel_handle.h>
#include <pdx/file_handle.h>
#include <pdx/rpc/remote_method.h>
#include <pdx/rpc/serializable.h>
#include <private/dvr/ion_buffer.h>

namespace android {
namespace dvr {

template <typename FileHandleType>
class NativeBufferHandle {
 public:
  NativeBufferHandle() { Clear(); }
  NativeBufferHandle(const IonBuffer& buffer, int id)
      : id_(id),
        stride_(buffer.stride()),
        width_(buffer.width()),
        height_(buffer.height()),
        format_(buffer.format()),
        usage_(buffer.usage()) {
    // Populate the fd and int vectors: native_handle->data[] is an array of fds
    // followed by an array of opaque ints.
    const int fd_count = buffer.handle()->numFds;
    const int int_count = buffer.handle()->numInts;
    for (int i = 0; i < fd_count; i++) {
      fds_.emplace_back(FileHandleType::AsDuplicate(buffer.handle()->data[i]));
    }
    for (int i = 0; i < int_count; i++) {
      opaque_ints_.push_back(buffer.handle()->data[fd_count + i]);
    }
  }
  NativeBufferHandle(NativeBufferHandle&& other) = default;

  // Imports the native handle into the given IonBuffer instance.
  int Import(IonBuffer* buffer) {
    // This is annoying, but we need to convert the vector of FileHandles into a
    // vector of ints for the Import API.
    std::vector<int> fd_ints;
    for (const auto& fd : fds_)
      fd_ints.push_back(fd.Get());

    const int ret = buffer->Import(fd_ints.data(), fd_ints.size(),
                                   opaque_ints_.data(), opaque_ints_.size(),
                                   width_, height_, stride_, format_, usage_);
    if (ret < 0)
      return ret;

    // Import succeeded, release the file handles which are now owned by the
    // IonBuffer and clear members.
    for (auto& fd : fds_)
      fd.Release();
    opaque_ints_.clear();
    Clear();

    return 0;
  }

  int id() const { return id_; }
  size_t IntCount() const { return opaque_ints_.size(); }
  size_t FdCount() const { return fds_.size(); }

 private:
  int id_;
  int stride_;
  int width_;
  int height_;
  int format_;
  int usage_;
  std::vector<int> opaque_ints_;
  std::vector<FileHandleType> fds_;

  void Clear() { id_ = stride_ = width_ = height_ = format_ = usage_ = -1; }

  PDX_SERIALIZABLE_MEMBERS(NativeBufferHandle<FileHandleType>, id_, stride_,
                           width_, height_, format_, usage_, opaque_ints_,
                           fds_);

  NativeBufferHandle(const NativeBufferHandle&) = delete;
  void operator=(const NativeBufferHandle&) = delete;
};

template <typename FileHandleType>
class FenceHandle {
 public:
  FenceHandle() = default;
  explicit FenceHandle(int fence) : fence_{fence} {}
  explicit FenceHandle(FileHandleType&& fence) : fence_{std::move(fence)} {}
  FenceHandle(FenceHandle&&) = default;
  FenceHandle& operator=(FenceHandle&&) = default;

  explicit operator bool() const { return fence_.IsValid(); }

  const FileHandleType& get() const { fence_; }
  FileHandleType&& take() { return std::move(fence_); }

  int get_fd() const { return fence_.Get(); }
  void close() { fence_.Close(); }

  FenceHandle<pdx::BorrowedHandle> borrow() const {
    return FenceHandle<pdx::BorrowedHandle>(fence_.Borrow());
  }

 private:
  FileHandleType fence_;

  PDX_SERIALIZABLE_MEMBERS(FenceHandle<FileHandleType>, fence_);

  FenceHandle(const FenceHandle&) = delete;
  void operator=(const FenceHandle&) = delete;
};

using LocalFence = FenceHandle<pdx::LocalHandle>;
using BorrowedFence = FenceHandle<pdx::BorrowedHandle>;

// BufferHub Service RPC interface. Defines the endpoints, op codes, and method
// type signatures supported by bufferhubd.
struct BufferHubRPC {
  // Service path.
  static constexpr char kClientPath[] = "system/buffer_hub/client";

  // |BufferHubQueue| will keep track of at most this value of buffers.
  // Attempts at runtime to increase the number of buffers past this
  // will fail. Note that the value is in sync with |android::BufferQueue|, so
  // that slot id can be shared between |android::dvr::BufferHubQueueProducer|
  // and |android::BufferQueueProducer| which both implements the same
  // interface: |android::IGraphicBufferProducer|.
  static constexpr size_t kMaxQueueCapacity =
      android::BufferQueueDefs::NUM_BUFFER_SLOTS;

  // Op codes.
  enum {
    kOpCreateBuffer = 0,
    kOpCreatePersistentBuffer,
    kOpGetPersistentBuffer,
    kOpGetBuffer,
    kOpGetBuffers,
    kOpNewConsumer,
    kOpProducerMakePersistent,
    kOpProducerRemovePersistence,
    kOpProducerPost,
    kOpProducerGain,
    kOpConsumerAcquire,
    kOpConsumerRelease,
    kOpConsumerSetIgnore,
    kOpCreateProducerQueue,
    kOpCreateConsumerQueue,
    kOpProducerQueueAllocateBuffers,
    kOpProducerQueueDetachBuffer,
    kOpConsumerQueueImportBuffers,
  };

  // Aliases.
  using MetaData = pdx::rpc::BufferWrapper<std::uint8_t*>;
  using LocalChannelHandle = pdx::LocalChannelHandle;
  using LocalHandle = pdx::LocalHandle;
  using Void = pdx::rpc::Void;

  // Methods.
  PDX_REMOTE_METHOD(CreateBuffer, kOpCreateBuffer,
                    int(int width, int height, int format, int usage,
                        size_t meta_size_bytes, size_t slice_count));
  PDX_REMOTE_METHOD(CreatePersistentBuffer, kOpCreatePersistentBuffer,
                    int(const std::string& name, int user_id, int group_id,
                        int width, int height, int format, int usage,
                        size_t meta_size_bytes, size_t slice_count));
  PDX_REMOTE_METHOD(GetPersistentBuffer, kOpGetPersistentBuffer,
                    int(const std::string& name));
  PDX_REMOTE_METHOD(GetBuffer, kOpGetBuffer,
                    NativeBufferHandle<LocalHandle>(unsigned index));
  PDX_REMOTE_METHOD(GetBuffers, kOpGetBuffers,
                    std::vector<NativeBufferHandle<LocalHandle>>(Void));
  PDX_REMOTE_METHOD(NewConsumer, kOpNewConsumer, LocalChannelHandle(Void));
  PDX_REMOTE_METHOD(ProducerMakePersistent, kOpProducerMakePersistent,
                    int(const std::string& name, int user_id, int group_id));
  PDX_REMOTE_METHOD(ProducerRemovePersistence, kOpProducerRemovePersistence,
                    int(Void));
  PDX_REMOTE_METHOD(ProducerPost, kOpProducerPost,
                    int(LocalFence acquire_fence, MetaData));
  PDX_REMOTE_METHOD(ProducerGain, kOpProducerGain, LocalFence(Void));
  PDX_REMOTE_METHOD(ConsumerAcquire, kOpConsumerAcquire,
                    std::pair<LocalFence, MetaData>(std::size_t metadata_size));
  PDX_REMOTE_METHOD(ConsumerRelease, kOpConsumerRelease,
                    int(LocalFence release_fence));
  PDX_REMOTE_METHOD(ConsumerSetIgnore, kOpConsumerSetIgnore, int(bool ignore));

  // Buffer Queue Methods.
  PDX_REMOTE_METHOD(CreateProducerQueue, kOpCreateProducerQueue,
                    int(size_t meta_size_bytes, int usage_set_mask,
                        int usage_clear_mask, int usage_deny_set_mask,
                        int usage_deny_clear_mask));
  PDX_REMOTE_METHOD(CreateConsumerQueue, kOpCreateConsumerQueue,
                    std::pair<LocalChannelHandle, size_t>(Void));
  PDX_REMOTE_METHOD(ProducerQueueAllocateBuffers,
                    kOpProducerQueueAllocateBuffers,
                    std::vector<std::pair<LocalChannelHandle, size_t>>(
                        int width, int height, int format, int usage,
                        size_t slice_count, size_t buffer_count));
  PDX_REMOTE_METHOD(ProducerQueueDetachBuffer, kOpProducerQueueDetachBuffer,
                    int(size_t slot));
  PDX_REMOTE_METHOD(ConsumerQueueImportBuffers, kOpConsumerQueueImportBuffers,
                    std::vector<std::pair<LocalChannelHandle, size_t>>(Void));
};

}  // namespace dvr
}  // namespace android

#endif  // ANDROID_DVR_BUFFERHUB_RPC_H_