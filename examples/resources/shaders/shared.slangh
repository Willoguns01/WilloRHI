
#define WilloRHI_STORAGE_BUFFER_BINDING 0
#define WilloRHI_STORAGE_IMAGE_BINDING 1
#define WilloRHI_SAMPLED_IMAGE_BINDING 2
#define WilloRHI_SAMPLER_BINDING 3
#define WilloRHI_DEVICE_ADDRESS_BUFFER_BINDING 4

// read buffer
[[vk::binding(WilloRHI_STORAGE_BUFFER_BINDING, 0)]] ByteAddressBuffer ByteAddressBufferTable[];
extension ByteAddressBuffer {
    static ByteAddressBuffer Get(uint32_t bufferId) { return ByteAddressBufferTable[bufferId]; }
}

// read-write buffer
[[vk::binding(WilloRHI_STORAGE_BUFFER_BINDING, 0)]] RWByteAddressBuffer RWByteAddressBufferTable[];
[[vk::binding(WilloRHI_STORAGE_BUFFER_BINDING, 0)]] coherent RWByteAddressBuffer CoherentRWByteAddressBufferTable[];
extension RWByteAddressBuffer {
    static RWByteAddressBuffer Get(uint32_t bufferId) { return RWByteAddressBufferTable[bufferId]; }
    static RWByteAddressBuffer GetCoherent(uint32_t bufferId) { return CoherentRWByteAddressBufferTable[bufferId]; }
}

// read-write image
[[vk::binding(WilloRHI_STORAGE_IMAGE_BINDING, 0)]] RWTexture2D StorageImageTable[];
[[vk::binding(WilloRHI_STORAGE_IMAGE_BINDING, 0)]] RWTexture2DArray StorageImageArrayTable[];
extension RWTexture2D {
    static RWTexture2D Get(uint32_t textureId) { return StorageImageTable[textureId]; }
}
extension RWTexture2DArray {
    static RWTexture2DArray Get(uint32_t textureId) { return StorageImageArrayTable[textureId]; }
}

// sampled image
[[vk::binding(WilloRHI_SAMPLED_IMAGE_BINDING, 0)]] Texture2D SampledImageTable[];
[[vk::binding(WilloRHI_SAMPLED_IMAGE_BINDING, 0)]] Texture2DArray SampledImageArrayTable[];
extension Texture2D {
    static Texture2D Get(uint32_t textureId) { return SampledImageTable[textureId]; }
}
extension Texture2DArray {
    static Texture2DArray Get(uint32_t textureId) { return SampledImageArrayTable[textureId]; }
}

// sampler
[[vk::binding(WilloRHI_SAMPLER_BINDING, 0)]] SamplerState SamplerStateTable[];
extension SamplerState {
    static SamplerState Get(uint32_t samplerId) { return SamplerStateTable[samplerId]; }
}

// BDA buffer
[[vk::binding(WilloRHI_DEVICE_ADDRESS_BUFFER_BINDING, 0)]] StructuredBuffer<uint64_t> BufferDeviceAddressBuffer;
namespace WilloRHI {
    uint64_t BufferIdToAddress(uint32_t bufferId) { return BufferDeviceAddressBuffer[bufferId]; }
}
