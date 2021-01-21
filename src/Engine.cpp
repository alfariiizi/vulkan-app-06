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
    createMemoryAllocator();
    createSwapchainComponent();
    createRenderPass();
    createFramebuffers();
    createObjectToRender();
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

    /***************************************************/

    /**
      * @brief This image actually not part of swapchain image. It's image is for depth image
     */

    /**
     * @brief Init the image info
     */
    vk::Extent3D depthImageExtent { ScreenWidth, ScreenHeight, 1 };
    _depthFormat = vk::Format::eD32Sfloat; // most GPU support this format
    auto depthImageInfo = init::image::initImageInfo( _depthFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment, depthImageExtent );

    /**
     * @brief Init the vma
     */
    vma::AllocationCreateInfo depthImageAllocInfo {};
    depthImageAllocInfo.setUsage( vma::MemoryUsage::eGpuOnly ); // this because we use optimal tilling when use the image
    depthImageAllocInfo.setRequiredFlags( vk::MemoryPropertyFlagBits::eDeviceLocal );   // because "GPU only" and optimal tilling wkwkwk

    /**
     * @brief Creating the image
     */
    {
        auto temp = _allocator.createImage( depthImageInfo, depthImageAllocInfo );
        _depthImage.image = temp.first;
        _depthImage.allocation = temp.second;
    }

    /**
     * @brief Creating the image view
     */
    auto depthImageViewInfo = init::image::initImageViewInfo( _depthFormat, _depthImage.image, vk::ImageAspectFlagBits::eDepth );
    _depthImageView = _device->createImageView( depthImageViewInfo );

    /**
     * @brief Push to deletion
     */
    _mainDeletionQueue.pushFunction(
        [d = _device.get(), a = _allocator, i = _depthImage, iv = _depthImageView](){
            d.destroyImageView( iv );
            a.destroyImage( i.image, i.allocation );
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
     * @brief the renderpass will use this COLOR attachment.
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
     * @brief the renderpass will use this DEPTH attachment.
     */
    vk::AttachmentDescription depthAttachment {};
    depthAttachment.setFormat( _depthFormat );
    depthAttachment.setSamples( vk::SampleCountFlagBits::e1 );  // we won't use fancy sample for right now
    depthAttachment.setLoadOp( vk::AttachmentLoadOp::eClear );
    depthAttachment.setStoreOp( vk::AttachmentStoreOp::eStore );
    depthAttachment.setStencilLoadOp( vk::AttachmentLoadOp::eClear );
    depthAttachment.setStencilStoreOp( vk::AttachmentStoreOp::eDontCare );
    depthAttachment.setInitialLayout( vk::ImageLayout::eUndefined );
    depthAttachment.setFinalLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal );


    /**
     * @brief COLOR Attachment Reference, this is needed for creating Subpass
     */
    vk::AttachmentReference colorAttachmentRef {};
    //attachment number will index into the pAttachments array in the parent renderpass itself
    colorAttachmentRef.setAttachment( 0 );
    // this layout 'll be use during subpass that use this attachment ref
    colorAttachmentRef.setLayout( vk::ImageLayout::eColorAttachmentOptimal );

    /**
     * @brief DEPTH Attachment Reference, this is needed for creating Subpass
     */
    vk::AttachmentReference depthAttachmentRef {};
    depthAttachmentRef.setAttachment( 1 );
    depthAttachmentRef.setLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal );

    /**
     * @brief Subpass
     */
    vk::SubpassDescription subpass {};
    // this bind point could be graphics, compute, or maybe ray tracing
    subpass.setPipelineBindPoint( vk::PipelineBindPoint::eGraphics );
    // color attachment that 'll be used
    subpass.setColorAttachments( colorAttachmentRef );
    // depth attachment that 'll be used
    subpass.setPDepthStencilAttachment( &depthAttachmentRef );

    // vk::SubpassDependency subpassDependecy {};
    // subpassDependecy.setSrcSubpass( VK_SUBPASS_EXTERNAL );
    // subpassDependecy.setDstSubpass( 0 );
    // subpassDependecy.setSrcStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
    // // subpassDependecy.setSrcAccessMask( vk::AccessFlags() );
    // subpassDependecy.setDstStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
    // subpassDependecy.setDstAccessMask( vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite );

    std::vector<vk::AttachmentDescription> attachDescs = { colorAttachment, depthAttachment };

    vk::RenderPassCreateInfo renderPassInfo {};
    renderPassInfo.setAttachments( attachDescs );
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
            imageView,
            _depthImageView     // each framebuffer use the same depth image view
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

void Engine::draw( vk::CommandBuffer cmd ) 
{
    // /**
    //  * @brief Bind the graphics pipeline
    //  */
    // // _mainCommandBuffer->bindPipeline( 
    // //     vk::PipelineBindPoint::eGraphics,       // pipeline bind point (in the future, it could be compute pipeline)
    // //     _graphicsPipeline                       // the pipeline
    // // );
    // // _mainCommandBuffer->bindPipeline( vk::PipelineBindPoint::eGraphics, _graphicsTriangleMeshPipeline );
    // _mainCommandBuffer->bindPipeline( vk::PipelineBindPoint::eGraphics, _monkeyGraphicsPipeline );

    // vk::DeviceSize offset = 0;
    // // _mainCommandBuffer->bindVertexBuffers( 0, _triangleMesh.vertexBuffer.buffer, offset );
    // _mainCommandBuffer->bindVertexBuffers( 0, _monkeyMesh.vertexBuffer.buffer, offset );
    // // _mainCommandBuffer->bindVertexBuffers( 0, _monkeyMesh->getMesh().vertexBuffer.buffer, offset );


    // /**
    //  * @brief Math's thing hahaha
    //  */
    // glm::vec3 camPos = { 0.0f, 0.0f, -2.0f };
    // glm::mat4 view = glm::translate( glm::mat4(1.0f), camPos );
    // // camera projection
    // glm::mat4 projection = glm::perspective( glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f );
    // projection[1][1] *= -1.0f;
    // // model rotation
    // glm::mat4 model = glm::rotate( glm::mat4(1.0f), glm::radians( _frameNumber * 0.4f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

    // // calculate final matrix mesh
    // glm::mat4 matrixMesh = projection * view * model;
    
    // MeshPushConstant meshPushConstant;
    // meshPushConstant.renderMatrix = matrixMesh;

    // // _mainCommandBuffer->pushConstants<MeshPushConstant>( _meshPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, meshPushConstant );
    // _mainCommandBuffer->pushConstants<MeshPushConstant>( _monkeyPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, meshPushConstant );

    // /**
    //  * @brief Drawing
    //  */
    // // _mainCommandBuffer->draw(
    // //     3,      // vertex count ( triangle has 3 vertices, right )
    // //     1,      // instance count ( it's not the instance handle, but how many object will be draw, I'll draw 1 triangle, so just 1 instance )
    // //     0,      // first vertex ( I'll be draw the vertex from index 0 )
    // //     0       // first instance ( I'll be draw the instance from index 0 )
    // // );

    // // _mainCommandBuffer->draw(
    // //     _triangleMesh.vertices.size(),
    // //     1,
    // //     0,
    // //     0
    // // );
    
    // _mainCommandBuffer->draw( _monkeyMesh.vertices.size(), 1, 0, 0 );

    _sceneManag.drawObject( cmd );
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

    // clear value for color
    vk::ClearColorValue colorClearValue = { std::array<float, 4UL>{ 0.0f, 0.0f, 0.0f, 1.0f } };
    // clear value for depth
    vk::ClearDepthStencilValue depthStencilClearValue {};
    depthStencilClearValue.setDepth( 1.0f );

    std::vector<vk::ClearValue> clearValue = { colorClearValue, depthStencilClearValue };

    renderPassBeginInfo.setClearValues( clearValue );

    _mainCommandBuffer->beginRenderPass( renderPassBeginInfo, vk::SubpassContents::eInline );

    draw( _mainCommandBuffer.get() );

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

void Engine::createObjectToRender() 
{
    createMeshes();
    createMaterials();
    initRenderObject();
}

void Engine::createMaterials() 
{
    defaultMaterial();
}

void Engine::initRenderObject() 
{
    RenderObject monkey;
    monkey.pMesh = _sceneManag.getPMehs( "monkey" );
    assert( monkey.pMesh != nullptr );
    monkey.pMaterial = _sceneManag.getPMaterial( "defaultMaterial" );
    assert( monkey.pMaterial != nullptr );
    monkey.transformMatrix = glm::mat4( 1.0f );
    _sceneManag.pushRenderableObject( monkey );

    for( int x = -20; x <= 20; ++x )
    {
        for( int y = -20; y <= 20; ++y )
        {
            RenderObject triangle;
            triangle.pMesh = _sceneManag.getPMehs( "triangle" );
            assert( triangle.pMesh != nullptr );
            triangle.pMaterial = _sceneManag.getPMaterial( "defaultMaterial" );
            assert( triangle.pMaterial != nullptr );

            glm::mat4 translation = glm::translate( glm::mat4{ 1.0f }, glm::vec3{ x, 0, y } );
            glm::mat4 scale = glm::scale( glm::mat4{ 1.0f }, glm::vec3{ 0.2f, 0.2f, 0.2f } );
            triangle.transformMatrix = translation * scale;

            _sceneManag.pushRenderableObject( triangle );
        }
    }
}

void Engine::createMeshes() 
{
    createTriangleMesh();
    createMonkeyMesh();
}

void Engine::createTriangleMesh() 
{
    Mesh triangleMesh;
    triangleMesh.vertices.resize( 3 );

    /**
     * @brief Vertex Position
     */
    triangleMesh.vertices[0].position = { 1.0f, 1.0f, 0.5f };
    triangleMesh.vertices[1].position = { -1.0f, 1.0f, 0.5f };
    triangleMesh.vertices[2].position = { 0.0f, -1.0f, 0.5f };

    /**
     * @brief Vertex Color
     */
    triangleMesh.vertices[0].color = { 1.0f, 0.0f, 0.0f };
    triangleMesh.vertices[1].color = { 0.0f, 1.0f, 0.0f };
    triangleMesh.vertices[2].color = { 0.0f, 0.0f, 1.0f };

    uploadMesh( triangleMesh );

    _sceneManag.createMesh( triangleMesh, "triangle" );
}

void Engine::createMonkeyMesh() 
{
    Mesh monkeyMesh;
    monkeyMesh.loadFromObj( "resources/monkey_smooth.obj" );
    uploadMesh( monkeyMesh );

    _sceneManag.createMesh( monkeyMesh, "monkey" );
}

void Engine::defaultMaterial() 
{
    vk::PipelineLayout layout;
    vk::Pipeline pipeline;

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
        layout = _device->createPipelineLayout( pipelineLayoutInfo );
    } ENGINE_CATCH
    _mainDeletionQueue.pushFunction(
        [d = _device.get(), pl = layout](){
            d.destroyPipelineLayout( pl );
        }
    );

    /**
     * @brief Depth Stencil Info
     */
    auto depthStencilInfo = GraphicsPipeline::createDepthStencilInfo( true, true, vk::CompareOp::eLessOrEqual );

    /**
     * @brief init default pipeline builder
     */
    GraphicsPipeline builder;
    builder.init( _device.get(), "shaders/tri_mesh_vertex.spv", "shaders/frag.spv", _swapchainExtent );

    /**
     * @brief Vertex Input state info
     */
    builder.m_vertexInputDesc = Vertex::getVertexInputDescription();
    builder.m_vertexInputStateInfo.setVertexBindingDescriptions( builder.m_vertexInputDesc.bindings );
    builder.m_vertexInputStateInfo.setVertexAttributeDescriptions( builder.m_vertexInputDesc.attributs );

    /**
     * @brief initialize the depth stencil
     */
    builder.m_useDepthStencil = true;
    builder.m_depthStencilStateInfo = depthStencilInfo;

    builder.createGraphicsPipeline( _renderPass, layout, _mainDeletionQueue );
    pipeline = builder.m_graphicsPipeline;

    _sceneManag.createMaterial( pipeline, layout, "defaultMaterial" );
}