#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/transform.hpp>

#include "vk_mem_alloc.hpp"

struct AllocatedBuffer
{
    vk::Buffer buffer;
    vma::Allocation allocation;

    static AllocatedBuffer createBuffer( size_t allocSize, vk::BufferUsageFlags bufferUsage, vma::Allocator allocator, vma::MemoryUsage memoryUsage );
};

struct AllocatedImage
{
    vk::Image image;
    vma::Allocation allocation;
};

struct VertexInputDescription
{
    std::vector<vk::VertexInputBindingDescription> bindings;
    std::vector<vk::VertexInputAttributeDescription> attributs;

    vk::PipelineVertexInputStateCreateFlags flags = vk::PipelineVertexInputStateCreateFlags();
};

struct MeshPushConstant         // 16 bytes + 64 bytes = 80 bytes (max size yg dpt di smpn sbg push constant adalah 128 bytes)
{
    glm::vec4 data;             // 4 floats = 16 bytes
    glm::mat4 renderMatrix;    // 16 floats = 64 bytes
};

// namespace dsc   // dsc = descriptor
// {
struct GpuCameraData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewproj;
};
struct GpuSceneParameterData     // despite the name is "Scene" but this struct has no relationship with SceneManagement
{
    glm::vec4 fogColor;     // w is for exponent
    glm::vec4 fogDistance;  // x for min, y for max, z and w are unused
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
};
// } // namespace dsc

struct GpuObjectData
{
    glm::mat4 modelMatrix;
};

struct SceneParameter
{
    GpuSceneParameterData sceneParameter;
    AllocatedBuffer allocationBuffer;
};

struct FrameData
{
    vk::Semaphore presentSemaphore;
    vk::Semaphore renderSemaphore;
    vk::Fence renderFence;

    vk::CommandPool commandPool;
    vk::CommandBuffer mainCommandBuffer;

    AllocatedBuffer cameraBuffer;
    vk::DescriptorSet globalDescriptorSet;

    AllocatedBuffer objectBuffer;
    vk::DescriptorSet objectDescriptorSet;
};

/*
 * MeshPushConstant
 * GpuCameraData
 * GpuSceneData
 * 
 * are must have the same byte size as on the shader file side
 */