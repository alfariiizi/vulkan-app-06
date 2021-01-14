#pragma once

#include <vulkan/vulkan.hpp>

#include "DeletionQueue.hpp"

class GraphicsPipeline
{
public:
    void init(  const vk::Device& device,
                const vk::RenderPass& renderpass,
                const std::string& vertFile, 
                const std::string& fragFile,
                const vk::Extent2D& windowExtent
    );

protected:
    virtual void createShaderStage( const std::string& vertFile, const std::string& fragFile );
    virtual void createVertexInputState();
    virtual void createInputAssemblyState();
    virtual void createViewportState( const vk::Extent2D& windowExtent );
    virtual void createRasterizationState();
    virtual void createMultisampleState();
    virtual void createColorBlendState();
    virtual void createPipelineLayout();
    void createGraphicsPipeline( const vk::RenderPass& renderpass );

public:
    vk::PipelineLayout getPipelineLayout() const;
    vk::Pipeline getGraphicsPipeline() const;
    DeletionQueue getDeletionQueue() const;

protected:
    vk::UniqueShaderModule m_vertShader {};
    vk::UniqueShaderModule m_fragShader {};
    std::vector<vk::PipelineShaderStageCreateInfo> m_shaderStagesInfo {};

protected:
    vk::VertexInputBindingDescription m_vertexInputBindingDesc {};
    vk::VertexInputAttributeDescription m_vertexInputAttributeDesc {};
    vk::PipelineVertexInputStateCreateInfo m_vertexInputStateInfo {};

protected:
    vk::PipelineInputAssemblyStateCreateInfo m_inputAssemblyStateInfo {};

protected:
    vk::Viewport m_viewport {};
    vk::Rect2D m_scissor {};
    vk::PipelineViewportStateCreateInfo m_viewportStateInfo {};

protected:
    vk::PipelineRasterizationStateCreateInfo m_rasterizationStateInfo {};

protected:
    vk::PipelineMultisampleStateCreateInfo m_multisampleStateInfo {};

protected:
    vk::PipelineColorBlendAttachmentState m_colorBlendAttachment {};
    vk::PipelineColorBlendStateCreateInfo m_colorBlendStateInfo {};

protected:
    vk::PushConstantRange m_pushConstant {};
    vk::PipelineLayoutCreateInfo m_pipelineLayoutInfo {};
    vk::PipelineLayout m_pipelineLayout;

protected:
    vk::GraphicsPipelineCreateInfo m_graphicsPipelineInfo {};
    vk::Pipeline m_graphicsPipeline;

protected:
    vk::Device device;
    DeletionQueue m_deletionQueue;
    bool hasInit = false;
};
