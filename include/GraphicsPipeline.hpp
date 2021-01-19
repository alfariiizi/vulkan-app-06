#pragma once

#include <vulkan/vulkan.hpp>

#include "DeletionQueue.hpp"
#include "utils.hpp"

class GraphicsPipeline
{
public:
    void init(  vk::Device device,
                const std::string& vertFile, 
                const std::string& fragFile,
                const vk::Extent2D& windowExtent
    );

private:
    void createShaderStage( const std::string& vertFile, const std::string& fragFile );
    void createVertexInputState();
    void createInputAssemblyState();
    void createViewportState( const vk::Extent2D& windowExtent );
    void createRasterizationState();
    void createMultisampleState();
    void createColorBlendState();

public:
    static vk::PipelineDepthStencilStateCreateInfo createDepthStencilInfo( bool bDepthTest, bool bDepthWrite, vk::CompareOp compareOp );

public:
    void createGraphicsPipeline( const vk::RenderPass& renderpass, vk::PipelineLayout pipelineLayout, DeletionQueue& deletor );

public:
    vk::PipelineLayout getPipelineLayout() const;
    vk::Pipeline getGraphicsPipeline() const;

public:
    vk::UniqueShaderModule m_vertShaderModule;
    vk::UniqueShaderModule m_fragShaderModule;
    std::vector<vk::PipelineShaderStageCreateInfo> m_shaderStagesInfo {};

public:
    VertexInputDescription m_vertexInputDesc;
    vk::PipelineVertexInputStateCreateInfo m_vertexInputStateInfo {};

public:
    vk::PipelineInputAssemblyStateCreateInfo m_inputAssemblyStateInfo {};

public:
    vk::Viewport m_viewport {};
    vk::Rect2D m_scissor {};
    vk::PipelineViewportStateCreateInfo m_viewportStateInfo {};

public:
    vk::PipelineRasterizationStateCreateInfo m_rasterizationStateInfo {};

public:
    vk::PipelineMultisampleStateCreateInfo m_multisampleStateInfo {};

public:
    vk::PipelineColorBlendAttachmentState m_colorBlendAttachment {};
    vk::PipelineColorBlendStateCreateInfo m_colorBlendStateInfo {};

public:
    vk::PipelineDepthStencilStateCreateInfo m_depthStencilStateInfo {};
    bool m_useDepthStencil = false;

public:
    vk::GraphicsPipelineCreateInfo m_graphicsPipelineInfo {};
    vk::Pipeline m_graphicsPipeline;

public:
    vk::Device device;
    bool hasInit = false;
};
