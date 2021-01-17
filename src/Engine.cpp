#include "Engine.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vk_mem_alloc.hpp"

#include "Vulkan_Init.hpp"
#include "GraphicsPipeline.hpp"

#include <iostream>
#include <assert.h>
#include <fstream>

#ifndef ENGINE_CATCH
#define ENGINE_CATCH                            \
    catch( const vk::SystemError& err )         \
    {                                           \
        throw std::runtime_error( err.what() ); \
    }
#endif

Engine::Engine() 
{
    run();
}

Engine::~Engine() 
{
    cleanUp();
}

void Engine::run() 
{
    initWindow();
    initVulkan();
    mainLoop();
}

void Engine::initWindow() 
{
    glfwInit();
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

    _window = glfwCreateWindow( ScreenWidth, ScreenHeight, "Vulkan Application", nullptr, nullptr );
}

void Engine::initVulkan() 
{
    createMainVulkanComponent();
    createSwapchainComponent();
    createRenderPass();
    createFramebuffers();
    createMemoryAllocator();
    createGraphicsPipeline();
    createCommandComponent();
    createSyncObject();
}

void Engine::mainLoop() 
{
    while( !glfwWindowShouldClose( _window ) )
    {
        glfwPollEvents();
        beginFrame();
        record();
        endFrame();
        _frameNumber++;
        if( _frameNumber >= (UINT32_MAX - 1) )
            _frameNumber = 0;
    }
}

void Engine::cleanUp() 
{
    _graphicsQueue.waitIdle();
    _presentQueue.waitIdle();
    _device->waitIdle();

    _mainDeletionQueue.flush();

    utils::DestroyDebugUtilsMessengerEXT( _instance.get(), _debugUtilsMessenger, nullptr );

    vkDestroySurfaceKHR( _instance.get(), _surface, nullptr );
    glfwDestroyWindow( _window );
    glfwTerminate();
}

void Engine::createMainVulkanComponent() 
{
    /**
     * @brief Instance
     */
    _instance = init::createInstance();

    /**
     * @brief Debug Utils Messenger
     */
    init::createDebugUtilsMessengerInfo( _instance.get(), _debugUtilsMessenger );

    /**
     * @brief Surface
     */
    _surface = init::createSurfce( _instance.get(), _window );

    /**
     * @brief Physical Device
     */
    _physicalDevice = init::pickPhysicalDevice( _instance.get(), _surface );

    /**
     * @brief Device and The Queues
     */
    _device = init::createDevice( _physicalDevice, _surface );
    {
        auto graphicsAndPresentQueueFamily = utils::FindQueueFamilyIndices( _physicalDevice, _surface );
        _graphicsQueue = _device->getQueue( graphicsAndPresentQueueFamily.graphicsAndPresentFamilyIndex()[0], 0 );
        _presentQueue = _device->getQueue( graphicsAndPresentQueueFamily.graphicsAndPresentFamilyIndex()[1], 0 );
    }
}

void Engine::createSwapchainComponent() 
{
    /**
     * @brief Swapchain, swapchain images, and swapchain image views
     */
    _swapchain = init::sc::createSwapchain( _physicalDevice, _device.get(), _surface );
    _mainDeletionQueue.pushFunction( [=](){ _device->destroySwapchainKHR( _swapchain ); } );
    {
        auto surfaceCapability = _physicalDevice.getSurfaceCapabilitiesKHR( _surface );
        auto surfaceFormats = _physicalDevice.getSurfaceFormatsKHR( _surface );
        auto surfaceExtent = utils::sc::chooseSurfaceExtent( surfaceCapability );
        auto surfaceFormat = utils::sc::chooseSurfaceFormat( surfaceFormats );
        _swapchainExtent = surfaceExtent;
        _swapchainFormat = surfaceFormat.format;
    }
    init::sc::retrieveImagesAndCreateImageViews( _device.get(), _swapchain, _swapchainFormat, _swapchainImages, _swapchainImageViews );
    _mainDeletionQueue.pushFunction( 
        [d = _device.get(), imageViews = _swapchainImageViews]{
            for( auto& imageView : imageViews )
                d.destroyImageView( imageView );
        }
     );
}

void Engine::createCommandComponent() 
{
    _commandPool = init::cm::createCommandPool( _physicalDevice, _surface, _device.get() );
    _mainCommandBuffer = std::move( init::cm::createCommandBuffers( _device.get(), _commandPool.get(), vk::CommandBufferLevel::ePrimary, 1 ).front() );
}

void Engine::createRenderPass() 
{
    /**
     * @brief the renderpass will use this color attachment.
     */
    vk::AttachmentDescription colorAttachment {};
    //the attachment will have the format needed by the swapchain
    colorAttachment.setFormat           ( _swapchainFormat );
    //1 sample, we won't be doing MSAA
    colorAttachment.setSamples          ( vk::SampleCountFlagBits::e1 );
    // we Clear when this attachment is loaded
    colorAttachment.setLoadOp           ( vk::AttachmentLoadOp::eClear );
    // we keep the attachment stored when the renderpass ends
    colorAttachment.setStoreOp          ( vk::AttachmentStoreOp::eStore );
    //we don't care about stencil
    colorAttachment.setStencilLoadOp    ( vk::AttachmentLoadOp::eDontCare );
    colorAttachment.setStencilStoreOp   ( vk::AttachmentStoreOp::eDontCare );

    //we don't know or care about the starting layout of the attachment
    colorAttachment.setInitialLayout    ( vk::ImageLayout::eUndefined );

    //after the renderpass ends, the image has to be on a layout ready for display
    colorAttachment.setFinalLayout      ( vk::ImageLayout::ePresentSrcKHR );


    /**
     * @brief Color Attachment Reference, this is needed for creating Subpass
     */
    vk::AttachmentReference colorAttachmentRef {};
    //attachment number will index into the pAttachments array in the parent renderpass itself
    colorAttachmentRef.setAttachment( 0 );
    // this layout 'll be use during subpass that use this attachment ref
    colorAttachmentRef.setLayout( vk::ImageLayout::eColorAttachmentOptimal );


    /**
     * @brief Subpass
     */
    vk::SubpassDescription subpass {};
    // this bind point could be graphics, compute, or maybe ray tracing
    subpass.setPipelineBindPoint( vk::PipelineBindPoint::eGraphics );
    // color attachment that 'll be used
    subpass.setColorAttachments( colorAttachmentRef );

    // vk::SubpassDependency subpassDependecy {};
    // subpassDependecy.setSrcSubpass( VK_SUBPASS_EXTERNAL );
    // subpassDependecy.setDstSubpass( 0 );
    // subpassDependecy.setSrcStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
    // // subpassDependecy.setSrcAccessMask( vk::AccessFlags() );
    // subpassDependecy.setDstStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
    // subpassDependecy.setDstAccessMask( vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite );

    vk::RenderPassCreateInfo renderPassInfo {};
    renderPassInfo.setAttachments( colorAttachment );
    renderPassInfo.setSubpasses( subpass );
    // renderPassInfo.setDependencies( subpassDependecy );

    try
    {
        _renderPass = _device->createRenderPass( renderPassInfo );
    } ENGINE_CATCH

    _mainDeletionQueue.pushFunction(
        [device = _device.get(), renderpass = _renderPass](){
            device.destroyRenderPass( renderpass );
        }
    );
}

void Engine::createFramebuffers() 
{
    _swapchainFramebuffers.reserve( _swapchainImageViews.size() );

    for( auto& imageView : _swapchainImageViews )
    {
        std::vector<vk::ImageView> attachments = {
            imageView
        };

        vk::FramebufferCreateInfo framebufferInfo {};
        framebufferInfo.setRenderPass( _renderPass );
        framebufferInfo.setAttachments( attachments );
        framebufferInfo.setWidth( _swapchainExtent.width );
        framebufferInfo.setHeight( _swapchainExtent.height );
        framebufferInfo.setLayers( 1 );

        try
        {
            _swapchainFramebuffers.emplace_back( _device->createFramebuffer( framebufferInfo ) );
        } ENGINE_CATCH
    }

    _mainDeletionQueue.pushFunction(
        [d = _device.get(), framebuffers = _swapchainFramebuffers](){
            for( auto& framebuffer : framebuffers )
                d.destroyFramebuffer( framebuffer );
        }
    );
}

void Engine::createSyncObject() 
{
    try
    {
        _renderSemaphore = _device->createSemaphoreUnique( {} );
        _presentSemaphore = _device->createSemaphoreUnique( {} );
        _renderFence = _device->createFenceUnique( vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled } );
    } ENGINE_CATCH

}


// void Engine::createGraphicsPipeline() 
// {
//     auto vertShaderCode = utils::gp::readFile( "shaders/vert.spv" );
//     auto fragShaderCode = utils::gp::readFile( "shaders/frag.spv" );

//     auto vertShaderModule = utils::gp::createShaderModule( _device.get(), vertShaderCode );
//     auto fragShaderModule = utils::gp::createShaderModule( _device.get(), fragShaderCode );

//     vk::PipelineShaderStageCreateInfo vertexShader {};
//     vertexShader.setStage( vk::ShaderStageFlagBits::eVertex );
//     vertexShader.setModule( vertShaderModule.get() );
//     vertexShader.setPName( "main" );

//     vk::PipelineShaderStageCreateInfo fragShader {};
//     fragShader.setStage( vk::ShaderStageFlagBits::eFragment );
//     fragShader.setModule( fragShaderModule.get() );
//     fragShader.setPName( "main" );


//     std::vector<vk::PipelineShaderStageCreateInfo> stages = { vertexShader, fragShader };


//     /**
//      * @brief Vertex Input State
//      */
//     vk::PipelineVertexInputStateCreateInfo vertexInputStateInfo {};
//     vertexInputStateInfo.setVertexBindingDescriptions       ( nullptr );
//     vertexInputStateInfo.setVertexAttributeDescriptions     ( nullptr );


//     /**
//      * @brief Input Assembly State
//      */
//     vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {};
//     inputAssemblyStateInfo.setTopology                 ( vk::PrimitiveTopology::eTriangleList );
//     inputAssemblyStateInfo.setPrimitiveRestartEnable   ( VK_FALSE );


//     /**
//      * @brief Viewport State
//      */
//     vk::Viewport viewport {};
//     viewport.setX       ( 0.0f );
//     viewport.setY       ( 0.0f );
//     viewport.setWidth   ( static_cast<float>( _swapchainExtent.width ) );
//     viewport.setHeight  ( static_cast<float>( _swapchainExtent.height ) );
//     viewport.setMinDepth( 0.0f );
//     viewport.setMaxDepth( 1.0f );

//     vk::Rect2D scissor {};
//     scissor.setOffset( vk::Offset2D{ 0, 0 } );
//     scissor.setExtent( _swapchainExtent );

//     vk::PipelineViewportStateCreateInfo viewportStateInfo{};
//     viewportStateInfo.setViewports  ( viewport );
//     viewportStateInfo.setScissors   ( scissor );


//     /**
//      * @brief Rasterization State
//      */
//     vk::PipelineRasterizationStateCreateInfo rasterizationStateInfo {};
//     rasterizationStateInfo.setDepthClampEnable          ( VK_FALSE );
//     rasterizationStateInfo.setRasterizerDiscardEnable   ( VK_FALSE );
//     rasterizationStateInfo.setPolygonMode               ( vk::PolygonMode::eFill );
//     rasterizationStateInfo.setCullMode                  ( vk::CullModeFlagBits::eBack );
//     rasterizationStateInfo.setFrontFace                 ( vk::FrontFace::eClockwise );
//     // depth bias. If this false, then 4 arguments ahead are ignored
//     rasterizationStateInfo.setDepthBiasEnable           ( VK_FALSE );
//     rasterizationStateInfo.setLineWidth                 ( 1.0f );


//     /**
//      * @brief Multisampling State
//      */
//     vk::PipelineMultisampleStateCreateInfo multisampleStateInfo {};
//     multisampleStateInfo.setRasterizationSamples( vk::SampleCountFlagBits::e1 );
//     multisampleStateInfo.setSampleShadingEnable ( VK_FALSE );


//     /**
//      * @brief Color Blend State
//      */
//     vk::PipelineColorBlendAttachmentState colorBlendAttachmentState {};
//     colorBlendAttachmentState.setBlendEnable( VK_FALSE );
//     colorBlendAttachmentState.setColorWriteMask( 
//         vk::ColorComponentFlagBits::eR 
//         | vk::ColorComponentFlagBits::eG
//         | vk::ColorComponentFlagBits::eB
//         | vk::ColorComponentFlagBits::eA 
//     );

//     vk::PipelineColorBlendStateCreateInfo colorBlendStateInfo {};
//     colorBlendStateInfo.setLogicOpEnable    ( VK_FALSE );
//     colorBlendStateInfo.setLogicOp          ( vk::LogicOp::eCopy );
//     colorBlendStateInfo.setAttachments      ( colorBlendAttachmentState );
//     colorBlendStateInfo.setBlendConstants   ( { 0.0f, 0.0f, 0.0f, 0.0f } );


//     vk::GraphicsPipelineCreateInfo gpi {};  // graphics pipeline info
//     gpi.setStages                  ( stages                     );
//     gpi.setPVertexInputState       ( &vertexInputStateInfo      );
//     gpi.setPInputAssemblyState     ( &inputAssemblyStateInfo    );
//     gpi.setPTessellationState      ( nullptr                    );
//     gpi.setPViewportState          ( &viewportStateInfo         );
//     gpi.setPRasterizationState     ( &rasterizationStateInfo    );
//     gpi.setPMultisampleState       ( &multisampleStateInfo      );
//     gpi.setPDepthStencilState      ( nullptr                    );
//     gpi.setPColorBlendState        ( &colorBlendStateInfo       );
//     gpi.setPDynamicState           ( nullptr                    );
//     gpi.setLayout                  ( _pipelineLayout            );
//     gpi.setRenderPass              ( _renderPass                );
//     gpi.setSubpass                 ( 0                          );
//     gpi.setBasePipelineHandle      ( nullptr                    );
//     gpi.setBasePipelineIndex       ( 0                          );

//     try
//     {
//         _graphicsPipeline = _device->createGraphicsPipeline( nullptr, gpi ).value;
//     } ENGINE_CATCH

//     _mainDeletionQueue.pushFunction(
//         [d = _device.get(), gp = _graphicsPipeline](){
//             d.destroyPipeline( gp );
//         }
//     );

// }

// void Engine::createSyncObject() 
// {
//     try
//     {
//         _presentSemaphore = _device->createSemaphoreUnique( {} );
//         _renderFence = _device->createFenceUnique( vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled } );
//     } ENGINE_CATCH
// }

void Engine::createMemoryAllocator() 
{
    vma::AllocatorCreateInfo allocatorInfo {};
    allocatorInfo.setInstance( _instance.get() );
    allocatorInfo.setPhysicalDevice( _physicalDevice );
    allocatorInfo.setDevice( _device.get() );

    try
    {
        _allocator = vma::createAllocator( allocatorInfo );
    } ENGINE_CATCH

    _mainDeletionQueue.pushFunction(
        [a = _allocator](){
            a.destroy();
        }
    );
}

void Engine::beginFrame() 
{
    vk::Result result;
    result = _device->waitForFences( _renderFence.get(), VK_TRUE, _timeOut );
    if( result != vk::Result::eSuccess )
        throw std::runtime_error( "Failed to wait for Fences" );

    _device->resetFences( _renderFence.get() );
}

void Engine::draw() 
{
    /**
     * @brief Bind the graphics pipeline
     */
    // _mainCommandBuffer->bindPipeline( 
    //     vk::PipelineBindPoint::eGraphics,       // pipeline bind point (in the future, it could be compute pipeline)
    //     _graphicsPipeline                       // the pipeline
    // );
    // _mainCommandBuffer->bindPipeline( vk::PipelineBindPoint::eGraphics, _graphicsTriangleMeshPipeline );
    _mainCommandBuffer->bindPipeline( vk::PipelineBindPoint::eGraphics, _monkeyGraphicsPipeline );

    vk::DeviceSize offset = 0;
    // _mainCommandBuffer->bindVertexBuffers( 0, _triangleMesh.vertexBuffer.buffer, offset );
    _mainCommandBuffer->bindVertexBuffers( 0, _monkeyMesh.vertexBuffer.buffer, offset );
    // _mainCommandBuffer->bindVertexBuffers( 0, _monkeyMesh->getMesh().vertexBuffer.buffer, offset );


    glm::vec3 camPos = { 0.0f, 0.0f, -2.0f };
    glm::mat4 view = glm::translate( glm::mat4(1.0f), camPos );
    // camera projection
    glm::mat4 projection = glm::perspective( glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f );
    projection[1][1] *= -1.0f;
    // model rotation
    glm::mat4 model = glm::rotate( glm::mat4(1.0f), glm::radians( _frameNumber * 0.4f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

    // calculate final matrix mesh
    glm::mat4 matrixMesh = projection * view * model;
    
    MeshPushConstant meshPushConstant;
    meshPushConstant.renderMatrix = matrixMesh;

    // _mainCommandBuffer->pushConstants<MeshPushConstant>( _meshPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, meshPushConstant );
    _mainCommandBuffer->pushConstants<MeshPushConstant>( _monkeyPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, meshPushConstant );

    /**
     * @brief Drawing
     */
    // _mainCommandBuffer->draw(
    //     3,      // vertex count ( triangle has 3 vertices, right )
    //     1,      // instance count ( it's not the instance handle, but how many object will be draw, I'll draw 1 triangle, so just 1 instance )
    //     0,      // first vertex ( I'll be draw the vertex from index 0 )
    //     0       // first instance ( I'll be draw the instance from index 0 )
    // );

    // _mainCommandBuffer->draw(
    //     _triangleMesh.vertices.size(),
    //     1,
    //     0,
    //     0
    // );
    
    _mainCommandBuffer->draw( _monkeyMesh.vertices.size(), 1, 0, 0 );
    // _mainCommandBuffer->draw( _monkeyMesh->getMesh().vertices.size(), 1, 0, 0 );
}

void Engine::record() 
{
    _imageIndex = _device->acquireNextImageKHR( _swapchain, 
                            _timeOut, 
                            _presentSemaphore.get(), 
                            nullptr 
    ).value;

    _mainCommandBuffer->reset();

    vk::CommandBufferBeginInfo beginInfo {};
    beginInfo.setFlags( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );    // this flag is important too, but right now I'll not use that
    beginInfo.setPInheritanceInfo( nullptr );   // this pInheritance info is only used for secondary cmd buffer

    try
    {
        _mainCommandBuffer->begin( beginInfo );
    } ENGINE_CATCH


    /**
     * @brief Begin to Record the render pass
     */
    vk::RenderPassBeginInfo renderPassBeginInfo {};
    renderPassBeginInfo.setRenderPass( _renderPass );
    renderPassBeginInfo.setFramebuffer( _swapchainFramebuffers[_imageIndex] );
    renderPassBeginInfo.setRenderArea( 
        vk::Rect2D{ 
            { 0, 0 },           // offset
            _swapchainExtent    // extent
            }
    );
    vk::ClearColorValue blackMoreGreen { std::array<float, 4UL>{ 0.1f, 0.2f, 0.1f, 1.0f } };
    vk::ClearValue clearColor { blackMoreGreen };
    renderPassBeginInfo.setClearValues( clearColor );

    _mainCommandBuffer->beginRenderPass( renderPassBeginInfo, vk::SubpassContents::eInline );

    draw();

    /**
     * @brief End to Record the renderpass
     */
    _mainCommandBuffer->endRenderPass();


    /**
     * @brief Finish recording
     */
    try
    {
        _mainCommandBuffer->end();
    } ENGINE_CATCH
}

void Engine::endFrame() 
{
    /**
     * @brief Submit Info ( it could be graphics queue, compute queue, or maybe transfer queue )
     */
    vk::SubmitInfo submitInfo {};
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    submitInfo.setWaitSemaphores( _presentSemaphore.get() );
    submitInfo.setWaitDstStageMask( waitStage );
    submitInfo.setCommandBuffers( _mainCommandBuffer.get() );
    submitInfo.setSignalSemaphores( _renderSemaphore.get() );
    try
    {
        _graphicsQueue.submit( submitInfo, _renderFence.get() );
    } ENGINE_CATCH


    /**
     * @brief Present Info ( it's just the present queue )
     */
    vk::PresentInfoKHR presentInfo {};
    presentInfo.setWaitSemaphores( _renderSemaphore.get() );

    std::vector<vk::SwapchainKHR> swapchains = { _swapchain };
    presentInfo.setSwapchains( swapchains );
    presentInfo.setImageIndices( _imageIndex );

    auto result = _presentQueue.presentKHR( presentInfo );
    if( result != vk::Result::eSuccess )
        throw std::runtime_error( "Failed to presenting (_presentQueue)" );
}

void Engine::loadTriangleMesh() 
{
    _triangleMesh.vertices.resize( 3 );

    /**
     * @brief Vertex Position
     */
    _triangleMesh.vertices[0].position = { 0.0, -0.5f, 0.0f };
    _triangleMesh.vertices[1].position = { 0.5, 0.5f, 0.0f };
    _triangleMesh.vertices[2].position = { -0.5, 0.5f, 0.0f };

    /**
     * @brief Vertex Color
     */
    _triangleMesh.vertices[0].color = { 1.0f, 0.0f, 0.0f };
    _triangleMesh.vertices[1].color = { 0.0f, 1.0f, 0.0f };
    _triangleMesh.vertices[2].color = { 0.0f, 0.0f, 1.0f };

    // _monkeyMesh.loadFromObj( "assets/monkey_smooth.obj" );

    // uploadMesh( _triangleMesh );
    // uploadMesh( _monkeyMesh );
}

void Engine::uploadMesh(Mesh& mesh) 
{
    size_t size = mesh.vertices.size() * sizeof(Vertex);

    vk::BufferCreateInfo bufferInfo {};
    bufferInfo.setSize( size );
    bufferInfo.setUsage( vk::BufferUsageFlagBits::eVertexBuffer );

    vma::AllocationCreateInfo allocInfo {};
    allocInfo.setUsage( vma::MemoryUsage::eCpuToGpu );

    auto buffer = _allocator.createBuffer( bufferInfo, allocInfo );
    mesh.vertexBuffer.buffer = buffer.first;
    mesh.vertexBuffer.allocation = buffer.second;
    _mainDeletionQueue.pushFunction(
        [a = _allocator, m = mesh](){
            a.destroyBuffer( m.vertexBuffer.buffer, m.vertexBuffer.allocation );
        }
    );

    void* data = _allocator.mapMemory( mesh.vertexBuffer.allocation );
    memcpy( data, mesh.vertices.data(), size );
    _allocator.unmapMemory( mesh.vertexBuffer.allocation );
}

void Engine::createGraphicsPipeline() 
{
    // createTriangleMeshPipeline();
    createMonkeyMeshPipeline();
}

void Engine::createTriangleMeshPipeline() 
{
    // GraphicsPipeline builder;
    // builder.init( _device.get(), "shaders/vert.spv", "shaders/frag.spv", _swapchainExtent, _mainDeletionQueue );

    // /**
    //  * @brief Vertex Input state
    //  */
    // builder.m_vertexInputDesc = Vertex::getVertexInputDescription();
    // builder.m_vertexInputStateInfo.setVertexBindingDescriptions( builder.m_vertexInputDesc.bindings );
    // builder.m_vertexInputStateInfo.setVertexAttributeDescriptions( builder.m_vertexInputDesc.attributs );

    // builder.createGraphicsPipeline( _renderPass, _mainDeletionQueue );
    // _graphicsTriangleMeshPipeline = builder.m_graphicsPipeline;

    // /**
    //  * @brief Vertex Position
    //  */
    // _triangleMesh.vertices[0].position = { 0.0, -0.5f, 0.0f };
    // _triangleMesh.vertices[1].position = { 0.5, 0.5f, 0.0f };
    // _triangleMesh.vertices[2].position = { -0.5, 0.5f, 0.0f };

    // /**
    //  * @brief Vertex Color
    //  */
    // _triangleMesh.vertices[0].color = { 1.0f, 0.0f, 0.0f };
    // _triangleMesh.vertices[1].color = { 0.0f, 1.0f, 0.0f };
    // _triangleMesh.vertices[2].color = { 0.0f, 0.0f, 1.0f };
}

void Engine::createMonkeyMeshPipeline() 
{
    _monkeyMesh.loadFromObj("resources/monkey_smooth.obj");
    uploadMesh( _monkeyMesh );
    
    /**
     * @brief Pipeline layout info
     */
    vk::PushConstantRange pushConstant {};
    pushConstant.setOffset( 0 );
    pushConstant.setSize( sizeof( MeshPushConstant ) );
    pushConstant.setStageFlags( vk::ShaderStageFlagBits::eVertex );

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.setPushConstantRanges( pushConstant );
    try
    {
        _monkeyPipelineLayout = _device->createPipelineLayout( pipelineLayoutInfo );
    } ENGINE_CATCH
    _mainDeletionQueue.pushFunction(
        [d = _device.get(), pl = _monkeyPipelineLayout](){
            d.destroyPipelineLayout( pl );
        }
    );


    GraphicsPipeline builder;
    builder.init( _device.get(), "shaders/tri_mesh_vertex.spv", "shaders/frag.spv", _swapchainExtent, _mainDeletionQueue );

    /**
     * @brief Vertex Input state info
     */
    builder.m_vertexInputDesc = Vertex::getVertexInputDescription();
    builder.m_vertexInputStateInfo.setVertexBindingDescriptions( builder.m_vertexInputDesc.bindings );
    builder.m_vertexInputStateInfo.setVertexAttributeDescriptions( builder.m_vertexInputDesc.attributs );

    builder.createGraphicsPipeline( _renderPass, _monkeyPipelineLayout, _mainDeletionQueue );
    _monkeyGraphicsPipeline = builder.m_graphicsPipeline;
}
