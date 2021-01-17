#include "GraphicsPipeline.hpp"

#include "Initializer.hpp"
#include "Mesh.hpp"
#include "utils.hpp"

#ifndef ENGINE_CATCH
#define ENGINE_CATCH                            \
    catch( const vk::SystemError& err )         \
    {                                           \
        throw std::runtime_error( err.what() ); \
    }
#endif

void GraphicsPipeline::init( vk::Device device,
            const std::string& vertFile, 
            const std::string& fragFile,
            const vk::Extent2D& windowExtent,
            DeletionQueue& deletor
)
{
    this->device = device;
    createShaderStage( vertFile, fragFile );
    createVertexInputState();
    createInputAssemblyState();
    createViewportState( windowExtent );
    createRasterizationState();
    createMultisampleState();
    createColorBlendState();

    hasInit = true;
}

void GraphicsPipeline::createShaderStage( const std::string& vertFile, const std::string& fragFile ) 
{
    auto vertShaderCode = utils::gp::readFile( vertFile );
    auto fragShaderCode = utils::gp::readFile( fragFile );

    m_vertShaderModule = utils::gp::createShaderModule( device, vertShaderCode );
    m_fragShaderModule = utils::gp::createShaderModule( device, fragShaderCode );

    vk::PipelineShaderStageCreateInfo vertexShader {};
    vertexShader.setStage( vk::ShaderStageFlagBits::eVertex );
    vertexShader.setModule( m_vertShaderModule.get() );
    vertexShader.setPName( "main" );

    vk::PipelineShaderStageCreateInfo fragShader {};
    fragShader.setStage( vk::ShaderStageFlagBits::eFragment );
    fragShader.setModule( m_fragShaderModule.get() );
    fragShader.setPName( "main" );


    m_shaderStagesInfo = { vertexShader, fragShader };

    m_graphicsPipelineInfo.setStages( m_shaderStagesInfo );
}

void GraphicsPipeline::createVertexInputState() 
{
    // for basic drawing triangle, we not gonna use the vertex input description

    m_vertexInputStateInfo.setVertexBindingDescriptions( nullptr );
    m_vertexInputStateInfo.setVertexAttributeDescriptions( nullptr );

    m_graphicsPipelineInfo.setPVertexInputState( &m_vertexInputStateInfo );
}

void GraphicsPipeline::createInputAssemblyState() 
{
    m_inputAssemblyStateInfo.setTopology                 ( vk::PrimitiveTopology::eTriangleList );
    m_inputAssemblyStateInfo.setPrimitiveRestartEnable   ( VK_FALSE );

    m_graphicsPipelineInfo.setPInputAssemblyState( &m_inputAssemblyStateInfo );
}

void GraphicsPipeline::createViewportState( const vk::Extent2D& windowExtent ) 
{
    m_viewport.setX       ( 0.0f );
    m_viewport.setY       ( 0.0f );
    m_viewport.setWidth   ( static_cast<float>( windowExtent.width ) );
    m_viewport.setHeight  ( static_cast<float>( windowExtent.height ) );
    m_viewport.setMinDepth( 0.0f );
    m_viewport.setMaxDepth( 1.0f );

    m_scissor.setOffset( vk::Offset2D{ 0, 0 } );
    m_scissor.setExtent( windowExtent );

    m_viewportStateInfo.setViewports  ( m_viewport );
    m_viewportStateInfo.setScissors   ( m_scissor );

    m_graphicsPipelineInfo.setPViewportState( &m_viewportStateInfo );
}

void GraphicsPipeline::createRasterizationState() 
{
    m_rasterizationStateInfo.setDepthClampEnable          ( VK_FALSE );
    m_rasterizationStateInfo.setRasterizerDiscardEnable   ( VK_FALSE );
    m_rasterizationStateInfo.setPolygonMode               ( vk::PolygonMode::eFill );
    m_rasterizationStateInfo.setCullMode                  ( vk::CullModeFlagBits::eNone );
    m_rasterizationStateInfo.setFrontFace                 ( vk::FrontFace::eClockwise );
    // depth bias. If this false, then 4 arguments ahead are ignored
    m_rasterizationStateInfo.setDepthBiasEnable           ( VK_FALSE );
    m_rasterizationStateInfo.setLineWidth                 ( 1.0f );

    m_graphicsPipelineInfo.setPRasterizationState( &m_rasterizationStateInfo );
}

void GraphicsPipeline::createMultisampleState() 
{
    m_multisampleStateInfo.setRasterizationSamples( vk::SampleCountFlagBits::e1 );
    m_multisampleStateInfo.setSampleShadingEnable ( VK_FALSE );

    m_graphicsPipelineInfo.setPMultisampleState( &m_multisampleStateInfo );
}

void GraphicsPipeline::createColorBlendState() 
{
    m_colorBlendAttachment.setBlendEnable( VK_FALSE );
    m_colorBlendAttachment.setColorWriteMask( 
        vk::ColorComponentFlagBits::eR 
        | vk::ColorComponentFlagBits::eG
        | vk::ColorComponentFlagBits::eB
        | vk::ColorComponentFlagBits::eA 
    );

    m_colorBlendStateInfo.setLogicOpEnable    ( VK_FALSE );
    m_colorBlendStateInfo.setLogicOp          ( vk::LogicOp::eCopy );
    m_colorBlendStateInfo.setAttachments      ( m_colorBlendAttachment );
    m_colorBlendStateInfo.setBlendConstants   ( { 0.0f, 0.0f, 0.0f, 0.0f } );

    m_graphicsPipelineInfo.setPColorBlendState( &m_colorBlendStateInfo );
}

void GraphicsPipeline::createGraphicsPipeline( const vk::RenderPass& renderpass, vk::PipelineLayout pipelineLayout, DeletionQueue& deletor ) 
{
    assert( hasInit );
    m_graphicsPipelineInfo.setLayout( pipelineLayout );
    m_graphicsPipelineInfo.setPDepthStencilState( nullptr );
    m_graphicsPipelineInfo.setRenderPass              ( renderpass                 );
    m_graphicsPipelineInfo.setSubpass                 ( 0                          );
    m_graphicsPipelineInfo.setBasePipelineHandle      ( nullptr                    );
    m_graphicsPipelineInfo.setBasePipelineIndex       ( 0                          );

    auto success = device.createGraphicsPipeline( nullptr, m_graphicsPipelineInfo );
    if( success.result == vk::Result::eSuccess )
    {
        m_graphicsPipeline = success.value;
        deletor.pushFunction(
            [d = device, gp = m_graphicsPipeline](){
                d.destroyPipeline( gp );
            }
        );
    }
}

vk::Pipeline GraphicsPipeline::getGraphicsPipeline() const
{
    assert( hasInit );
    return m_graphicsPipeline;
}