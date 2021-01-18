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