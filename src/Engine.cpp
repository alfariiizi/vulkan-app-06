#include "Engine.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vk_mem_alloc.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
    }                                           \
    catch(const std::exception& e)              \
    {                                           \
        throw std::runtime_error(e.what());     \
    }
#endif

#define SIN( X ) sinf( glm::radians( X ) )
#define COS( X ) cosf( glm::radians( X ) )

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
    createCommandComponent();
    createSyncObject();
    createRenderPass();
    createFramebuffers();
    createObjectToRender();
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
    _physicalDeviceProperties = _physicalDevice.getProperties();
    std::cout << "The GPU has a minimum buffer allignment of : " << _physicalDeviceProperties.limits.minUniformBufferOffsetAlignment << "\n";
    std::cout << "Device name : " << _physicalDeviceProperties.deviceName << "\n"
            << "Driver version : " << _physicalDeviceProperties.driverVersion << "\n\n\n";

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
    for( size_t i = 0; i < FRAME_OVERLAP; ++i )
    {
        _frames[i].commandPool = init::cm::createCommandPool( _physicalDevice, _surface, _device.get() );
        _frames[i].mainCommandBuffer = init::cm::createCommandBuffers( _device.get(), _frames[i].commandPool, vk::CommandBufferLevel::ePrimary, 1 ).front();

        _mainDeletionQueue.pushFunction(
            [d = _device.get(), cp = _frames[i].commandPool](){
                d.destroyCommandPool( cp );
            }
        );
    }

    /**
     * @brief CommandPool that used for uploading context ( stagging buffer )
     * I assume that the index present queue are the same as index graphics queue.
     * Actually we just wanna use present queue.
     */
    _uploadContext.commandPool = init::cm::createCommandPool( _physicalDevice, _surface, _device.get() );
    _mainDeletionQueue.pushFunction(
        [d = _device.get(), cp = _uploadContext.commandPool](){
            d.destroyCommandPool( cp );
        }
    );
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
    for( size_t i = 0; i < FRAME_OVERLAP; ++i )
    {
        try
        {
            _frames[i].presentSemaphore = _device->createSemaphore( {} );
            _frames[i].renderSemaphore = _device->createSemaphore( {} );
            _frames[i].renderFence = _device->createFence( vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled } );

            _mainDeletionQueue.pushFunction(
                [ d = _device.get(), ps = _frames[i].presentSemaphore, rs = _frames[i].renderSemaphore, f = _frames[i].renderFence](){
                    d.destroySemaphore( ps );
                    d.destroySemaphore( rs );
                    d.destroyFence( f );
                }
            );
        } ENGINE_CATCH
    }

    /**
     * @brief Fence that used for uploading context ( stagging buffer )
     */
    _uploadContext.uploadFence = _device->createFence( {} );
    _mainDeletionQueue.pushFunction(
        [d = _device.get(), f = _uploadContext.uploadFence]() {
            d.destroyFence( f );
        }
    );
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
    result = _device->waitForFences( getCurrentFrame().renderFence, VK_TRUE, _timeOut );
    if( result != vk::Result::eSuccess )
        throw std::runtime_error( "Failed to wait for Fences" );

    _device->resetFences( getCurrentFrame().renderFence );
}

void Engine::draw( vk::CommandBuffer cmd ) 
{
    /**
     * @brief draw() Week Point
     * This draw function has weak point, i.e. this function just support for render with normal/dynamic uniform buffer.
     * So, the "defaultMateril" won't work if you decide to use defaultMaterial to one of your renderable object
     */

    /**
     * @brief Camera (Normal Uniform Buffer)
     */
    {
        // camera view
        glm::vec3 camPos = { 0.0f, -6.0f, -10.0f };
        glm::mat4 view = glm::translate( glm::mat4{ 1.0f }, camPos );
        glm::mat4 projection = glm::perspective( glm::radians(70.0f), 1700.0f/ 900.0f, 0.1f, 200.0f );
        projection[1][1] *= -1;
        // filling the GPU camera data
        GpuCameraData camData;
        camData.projection = projection;
        camData.view = view;
        camData.viewproj = projection * view;
        // mapping memory
        void* data = _allocator.mapMemory( getCurrentFrame().cameraBuffer.allocation );
        memcpy( data, &camData, sizeof(GpuCameraData) );
        _allocator.unmapMemory( getCurrentFrame().cameraBuffer.allocation );
    }

    int frameIndex = _frameNumber % FRAME_OVERLAP;
    /**
     * @brief Scene (Dyanamic Uniform Buffer)
     */
    {
        float framed = glm::radians( static_cast<float>( _frameNumber ) );
        _sceneParameter.sceneParameter.ambientColor = { sinf( framed ), 0, cosf( framed ), 1 };
        // mapping memory
        char* sceneData;
        auto result = _allocator.mapMemory( _sceneParameter.allocationBuffer.allocation, reinterpret_cast<void**>(&sceneData) );
        if( result != vk::Result::eSuccess )
            throw std::runtime_error( "Failed to mapping the Scene Parameter Buffer" );
        sceneData += padUniformBufferSize( sizeof(GpuSceneParameterData) ) * frameIndex;
        memcpy( sceneData, &_sceneParameter.sceneParameter, sizeof(GpuSceneParameterData) );
        _allocator.unmapMemory( _sceneParameter.allocationBuffer.allocation );
    }

    const auto currentFrame = getCurrentFrame();
    /**
     * @brief Object (Storage Buffer)
     * This technic is kinda similar to memcpy().
     * It will maybe more complex operation that needed if we use memcpy(), 
     * because we glm::mat4 struct is store on the different class (one stores in GpuObjectData, and another stores in RenderObject).
     * But, both classes are have the same struct, i.e. glm::mat4 (of course wkwk).
     * For that reason, we manually copy every glm::mat4 for one class that used for push constant, to other class that used for SSBO.
     * 
     * Why is it possible ?
     * Well, maybe it because we specified "std140" at shader (specifically, vertex shader).
     * So, first, it makes the data on the memory are laid out like it can access through the [] operator.
     * And, second, it makes the casting from "void*" to "GpuObjectData*" is perfect fit,
     * because we specified the range of the storage buffer (like 've explained in the cmd.draw() section).
     * 
     * Other possibilites, that, we can use pointer aritmathics to iterate through all the object that we'll be copied
     * if you not want to use [] operator.
     * 
     * There are just just happen error with scenario like this:
     * reinterpret_cast<GpuObjectData*>( data ) --> casting from void data;
     * reinterpret_cast<GpuObjectData*>( &data ) --> casting from void *data;
     * The first one will give the address of pointer itself.
     * And the second one will give the address that pointer is pointing to.
     * So, be carefull !.
     */
    {
        void* data = _allocator.mapMemory( currentFrame.objectBuffer.allocation );
        GpuObjectData* ssbo = reinterpret_cast<GpuObjectData*>( data );
        for( int i = 0; i < _sceneManag.renderable.size(); ++i )
        {
            ssbo[i].modelMatrix = _sceneManag.renderable[i].transformMatrix;
        }
        _allocator.unmapMemory( currentFrame.objectBuffer.allocation );
    }

    /**
     * @brief Draw the object
     */
    {
        Mesh* lastMesh = nullptr;
        Material* pLastMaterial = nullptr;

        uint32_t i = 0U;
        for( auto& object : _sceneManag.renderable )
        {
            /**
             * @brief Material's things
             */
            if( object.pMaterial != pLastMaterial ) // just bind if the materials is valid
            {
                cmd.bindPipeline( vk::PipelineBindPoint::eGraphics, object.pMaterial->pipeline );

                // this bind descriptor set is just for dynamic buffer. Normal buffer no need this bind.
                // It's makes sense, because the normal buffer just has static offset.
                // In the other hand, the dynamic uniform buffer has dynamic offset.
                // So, if you just want to make static offset, then just use the normal uniform buffer.
                // Normal uniform buffer "should be" faster than the dynamic one, but I'm not sure, 
                // I just predict it can be like the static memory and dynamic memory.
                // And, the dynamic uniform buffer need to be bind every time, 
                // so maybe that gonna make dynamic buffer more slower (?)
                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,           // pipeline bind point
                    object.pMaterial->layout,                   // pipeline layout
                    0,                                          // first descriptor set on the array of descriptor set
                    currentFrame.globalDescriptorSet,           // the descriptor set (this could be an array, that's why there are "first descriptor set" right above this paramter)
                    frameIndex * padUniformBufferSize(sizeof( GpuSceneParameterData ))  // dynamic offset
                );
                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    object.pMaterial->layout,
                    1,
                    currentFrame.objectDescriptorSet,
                    nullptr
                );
            }

            /**
             * @brief Push Contant's things
             * Push constant use pipeline layout to allocate the space, and pipeline layout is about "Material".
             * But, "Material" is about how the object will look, not about how object is represented.
             * And it's not about the vertices that make up the "Mesh".
             * So, in the RenderObject class, there are member variable called "transformMatrix".
             * That member variable is used for representing the Mesh object to the 3D world.
             * So, it's neither "Material" nor "Mesh", it's transformMatrix.
             * That's why I make about this separate section "Push Constant's things"
             */
            MeshPushConstant pushconstant;
            // final render matrix that will be calculated on the cpu
            // pushconstant.renderMatrix = projection * view * model;
            pushconstant.renderMatrix = object.transformMatrix;
            vk::DeviceSize offsetMaterial = 0;
            cmd.pushConstants<MeshPushConstant>( object.pMaterial->layout, vk::ShaderStageFlagBits::eVertex, offsetMaterial, pushconstant );

            /**
             * @brief Mesh's things
             */
            if( object.pMesh != lastMesh ) // just bind if the mesh is valid
            {
                vk::DeviceSize offsetMesh = 0;
                cmd.bindVertexBuffers( 0, object.pMesh->vertexBuffer.buffer, offsetMesh );
            }

            /**
             * @brief Finally, Drawing Current RenderObject to the 3D world
             * We want draw 1 instance ( 1 object ).
             * When initializing the buffer info in initDescriptor, 
             * we set the range is the size of the struct itself and the offset is 0.
             * From 0 to the offset is 1 object, right ?!.
             * Ok, let say the size of the struct is 16 byte, 
             * and there are 100 objects that we've inputed, then the total size is 1600 bytes.
             * The range byte of the first object is from byte 0 until byte 16, 
             * and the last object is from byte 1584 until byte 1600.
             * That object is called instance.
             * And then in the vertex shader or just shader, we do "gl_BaseInstance", 
             * that means we accessing those instance by specifing "first instance" parameter below
             * 
             * Note that this is not normal/dynamic uniform buffer, so we do not worrying about the minimum padding's things.
             */
            cmd.draw( 
                object.pMesh->vertices.size(),      // vertex count
                1,                                  // instance count
                0,                                  // first vertex
                i                                   // first instance
            );

            ++i;
        }
    }
}

void Engine::record() 
{
    _imageIndex = _device->acquireNextImageKHR( _swapchain, 
                            _timeOut, 
                            getCurrentFrame().presentSemaphore, 
                            nullptr 
    ).value;

    getCurrentFrame().mainCommandBuffer.reset();

    vk::CommandBufferBeginInfo beginInfo {};
    beginInfo.setFlags( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );    // this flag is important too, but right now I'll not use that
    beginInfo.setPInheritanceInfo( nullptr );   // this pInheritance info is only used for secondary cmd buffer

    try
    {
        getCurrentFrame().mainCommandBuffer.begin( beginInfo );
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

    getCurrentFrame().mainCommandBuffer.beginRenderPass( renderPassBeginInfo, vk::SubpassContents::eInline );

    draw( getCurrentFrame().mainCommandBuffer );

    /**
     * @brief End to Record the renderpass
     */
    getCurrentFrame().mainCommandBuffer.endRenderPass();


    /**
     * @brief Finish recording
     */
    try
    {
        getCurrentFrame().mainCommandBuffer.end();
    } ENGINE_CATCH
}

void Engine::endFrame() 
{
    /**
     * @brief Submit Info ( it could be graphics queue, compute queue, or maybe transfer queue )
     */
    vk::SubmitInfo submitInfo {};
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    submitInfo.setWaitSemaphores( getCurrentFrame().presentSemaphore );
    submitInfo.setWaitDstStageMask( waitStage );
    submitInfo.setCommandBuffers( getCurrentFrame().mainCommandBuffer );
    submitInfo.setSignalSemaphores( getCurrentFrame().renderSemaphore );
    try
    {
        _graphicsQueue.submit( submitInfo, getCurrentFrame().renderFence );
    } ENGINE_CATCH


    /**
     * @brief Present Info ( it's just the present queue )
     */
    vk::PresentInfoKHR presentInfo {};
    presentInfo.setWaitSemaphores( getCurrentFrame().renderSemaphore );

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
    vma::AllocationCreateInfo allocInfo {}; // informasi untuk membuat buffer

    /**
     * @brief Creating Stagging Buffer
     * Just use for Transfer Source
     * And just for CPU only visible
     */
    AllocatedBuffer staggingBuffer;
    {
        vk::BufferCreateInfo staggingBufferInfo {};
        staggingBufferInfo.setSize( size );
        staggingBufferInfo.setUsage( vk::BufferUsageFlagBits::eTransferSrc ); // Transfer Source

        allocInfo.setUsage( vma::MemoryUsage::eCpuOnly );   // CPU only visible

        auto stagBuff = _allocator.createBuffer( staggingBufferInfo, allocInfo );
        staggingBuffer.buffer = stagBuff.first;
        staggingBuffer.allocation = stagBuff.second;
    }

    /**
     * @brief Mapping buffer
     */
    {
        void* data = _allocator.mapMemory( staggingBuffer.allocation );
        memcpy( data, mesh.vertices.data(), size );
        _allocator.unmapMemory( staggingBuffer.allocation );
    }

    /**
     * @brief Creating Vertex Buffer that just device visible (GPU visible)
     * Use for Vertex Buffer pipeline (of course) and use as the Transfer Destination (the source transfer is stagging buffer)
     * This buffer is GPU only visible
     */
    {
        vk::BufferCreateInfo vertexBufferInfo {};
        vertexBufferInfo.setSize( size );
        // Vertex Buffer (that 'll be rendered) and Transfer Destination
        vertexBufferInfo.setUsage( vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst );

        allocInfo.setUsage( vma::MemoryUsage::eGpuOnly ); // GPU only visible

        auto vertBuff = _allocator.createBuffer( vertexBufferInfo, allocInfo );
        mesh.vertexBuffer.buffer = vertBuff.first;
        mesh.vertexBuffer.allocation = vertBuff.second;
        _mainDeletionQueue.pushFunction(
            [a = _allocator, m = mesh](){
                a.destroyBuffer( m.vertexBuffer.buffer, m.vertexBuffer.allocation );
            }
        );
        
        // Copying content of the stagging buffer (CPU only) to vertex buffer (GPU only)
        immediateSubmit(
            [sb = staggingBuffer, vb = mesh.vertexBuffer, s = size]( vk::CommandBuffer cmd ) {
                vk::BufferCopy copy {};
                copy.setSize( s );
                copy.setSrcOffset( 0 );
                copy.setDstOffset( 0 );
                cmd.copyBuffer( sb.buffer, vb.buffer, copy );
            }
        );
    }

    // immedietly destroy the stagging buffer
    _allocator.destroyBuffer( staggingBuffer.buffer, staggingBuffer.allocation );
}

void Engine::createObjectToRender() 
{
    createMeshes();
    initDescriptors();  // the descriptor set layout member variable is used when creating material
    createMaterials();
    loadImages();
    initRenderObject();
}

void Engine::createMaterials() 
{
    // defaultMaterial();
    colorMaterial();
}

void Engine::initRenderObject() 
{
    /**
     * @brief Monkey object
     */
    {
        RenderObject monkey;
        monkey.pMesh = _sceneManag.getPMehs( "monkey" );
        assert( monkey.pMesh != nullptr );
        monkey.pMaterial = _sceneManag.getPMaterial( "colorMaterial" );
        assert( monkey.pMaterial != nullptr );
        monkey.transformMatrix = glm::mat4( 1.0f );
        _sceneManag.pushRenderableObject( monkey );
    }

    /**
     * @brief Triangles object
     * (-20 until <= 20, is 21 step. 21 for x step and for y step, so maybe 21 * 21 = 441 triangle objects)
     */
    for( int x = -20; x <= 20; ++x )
    {
        for( int y = -20; y <= 20; ++y )
        {
            RenderObject triangle;
            triangle.pMesh = _sceneManag.getPMehs( "triangle" );
            assert( triangle.pMesh != nullptr );
            triangle.pMaterial = _sceneManag.getPMaterial( "colorMaterial" );
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

void Engine::loadImages() 
{
    Texture lostEmpire;

    lostEmpire.image = loadImageFromFile( "../resources/" );

    auto imageViewInfo = init::image::initImageViewInfo( vk::Format::eR8G8B8Srgb, lostEmpire.image.image, vk::ImageAspectFlagBits::eColor );

    try
    {
        lostEmpire.imageView = _device->createImageView( imageViewInfo );
    } ENGINE_CATCH

    _sceneManag.createTexture( lostEmpire, "lostEmpire" );
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
    pipelineLayoutInfo.setSetLayouts( _globalSetLayout );   // descriptor set layout must be initialized first at the order of initVulkan
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
    builder.init( _device.get(), "shaders/vertex_shader.spv", "shaders/frag.spv", _swapchainExtent );

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

void Engine::colorMaterial() 
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
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = { _globalSetLayout, _objectSetLayout };
    pipelineLayoutInfo.setSetLayouts( descriptorSetLayouts );   // descriptor set layout must be initialized first at the order of initVulkan
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
    builder.init( _device.get(), "shaders/vertex_shader.spv", "shaders/fragment_shader.spv", _swapchainExtent );

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

    _sceneManag.createMaterial( pipeline, layout, "colorMaterial" );
}

void Engine::initDescriptors() 
{
    /**
     * @brief Create Descriptor pool
     */
    {
        uint32_t maxSets = 10;
        std::vector<vk::DescriptorPoolSize> sizes;

        // this Descriptor Pool will be hold 10 Uniform Buffer.
        vk::DescriptorPoolSize uniformbuffer {};
        uniformbuffer.setType( vk::DescriptorType::eUniformBuffer );
        uniformbuffer.setDescriptorCount( 10 );
        sizes.emplace_back( uniformbuffer );

        // the Desc Pool will be hold 10 Dynamic Uniform Buffer.
        vk::DescriptorPoolSize dynamicUniformBuffer {};
        dynamicUniformBuffer.setType( vk::DescriptorType::eUniformBufferDynamic );
        dynamicUniformBuffer.setDescriptorCount( 10 );
        sizes.emplace_back( dynamicUniformBuffer );

        // the pool will be hold 10 Storage Buffer.
        vk::DescriptorPoolSize storageBuffer {};
        storageBuffer.setType( vk::DescriptorType::eStorageBuffer );
        storageBuffer.setDescriptorCount( 10 );
        sizes.emplace_back( storageBuffer );

        vk::DescriptorPoolCreateInfo descriptorPoolInfo {};
        descriptorPoolInfo.setMaxSets( maxSets ); // the maximum descriptor sets that can be store to the pool
        descriptorPoolInfo.setPoolSizes( sizes ); // the poolsize struct
        try
        {
            _descriptorPool = _device->createDescriptorPool( descriptorPoolInfo );
        } ENGINE_CATCH
        _mainDeletionQueue.pushFunction(
            [d = _device.get(), dp = _descriptorPool](){
                d.destroyDescriptorPool( dp );
            }
        );
    }

    /**
     * @brief Create Descriptor set layout
     */
    {
        {
            // Camera binding
            vk::DescriptorSetLayoutBinding camBinding = init::dsc::initDescriptorSetLayoutBinding(
                0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex
            );
            // Scene binding
            vk::DescriptorSetLayoutBinding sceneBinding = init::dsc::initDescriptorSetLayoutBinding(
                1, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
            );

            // just create 1 descriptor layout, because we just need 1 descriptor set
            vk::DescriptorSetLayoutCreateInfo setLayoutInfo {};
            std::vector<vk::DescriptorSetLayoutBinding> setLayoutBinding = { camBinding, sceneBinding };
            setLayoutInfo.setBindings( std::array<vk::DescriptorSetLayoutBinding, 2>{ camBinding, sceneBinding } );

            try
            {
                _globalSetLayout = _device->createDescriptorSetLayout( setLayoutInfo );
            } ENGINE_CATCH
            _mainDeletionQueue.pushFunction(
                [d = _device.get(), sl = _globalSetLayout](){
                    d.destroyDescriptorSetLayout( sl );
                }
            );
        }
        {
            auto objectBinding = init::dsc::initDescriptorSetLayoutBinding(
                0,                                  // binding
                vk::DescriptorType::eStorageBuffer, // descriptor type
                vk::ShaderStageFlagBits::eVertex    // shader stage
            );
            vk::DescriptorSetLayoutCreateInfo setLayoutInfo {};
            setLayoutInfo.setBindings( objectBinding );
            try
            {
                _objectSetLayout = _device->createDescriptorSetLayout( setLayoutInfo );
            } ENGINE_CATCH
            _mainDeletionQueue.pushFunction(
                [d = _device.get(), sl = _objectSetLayout](){
                    d.destroyDescriptorSetLayout( sl );
                }
            );
        }
    }

    /**
     * @brief Create Scene Parameter Buffer
     */
    std::cout << "Original Size: " << sizeof(GpuSceneParameterData) << "\n";
    std::cout << "Padding Size: " << padUniformBufferSize( sizeof(GpuSceneParameterData) ) << '\n';
    std::cout << "Min limit uniform size: " << _physicalDeviceProperties.limits.minUniformBufferOffsetAlignment << "\n\n";
    {
        const uint32_t sceneParameterBufferSize = FRAME_OVERLAP * padUniformBufferSize( sizeof(GpuSceneParameterData) );
        _sceneParameter.allocationBuffer = AllocatedBuffer::createBuffer( 
            sceneParameterBufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer, 
            _allocator,
            vma::MemoryUsage::eCpuToGpu
        );
        _mainDeletionQueue.pushFunction(
            [ a = _allocator, b = _sceneParameter.allocationBuffer ](){
                a.destroyBuffer( b.buffer, b.allocation );
            }
        );
    }

    for( int i = 0; i < FRAME_OVERLAP; ++i )
    {
        /**
         * @brief Creating Buffer for each frame
         * 
         */
        const int maxObjectCount = 10000;
        {
            /**
             * @brief Camera Buffer
             * buffer type --> Uniform Buffer
             */
            {
                _frames[i].cameraBuffer = AllocatedBuffer::createBuffer( 
                    sizeof(GpuCameraData),
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    _allocator, vma::MemoryUsage::eCpuToGpu
                );
                _mainDeletionQueue.pushFunction(
                    [ a = _allocator, b = _frames[i].cameraBuffer ](){
                        a.destroyBuffer( b.buffer, b.allocation );
                    }
                );
            }
            /**
             * @brief Object Buffer
             * buffer type --> Storage Buffer
             * Note: Storage buffer is kinda like std::vector, it's can store a lot data to it, 
             *       but of course the price is it's slower than the normal/dynamic uniform buffer
             */
            {
                _frames[i].objectBuffer = AllocatedBuffer::createBuffer( 
                    sizeof(GpuObjectData) * maxObjectCount,
                    vk::BufferUsageFlagBits::eStorageBuffer,
                    _allocator,
                    vma::MemoryUsage::eCpuToGpu
                );
                _mainDeletionQueue.pushFunction(
                    [ a = _allocator, b = _frames[i].objectBuffer ](){
                        a.destroyBuffer( b.buffer, b.allocation );
                    }
                );
            }
        }

        /**
         * @brief Allocate Descriptor Set for each frame
         */
        {
            {
                vk::DescriptorSetAllocateInfo setAllocInfo {};
                setAllocInfo.setDescriptorPool( _descriptorPool );
                setAllocInfo.setSetLayouts( _globalSetLayout );
                setAllocInfo.setDescriptorSetCount( 1 );    // just 1 descriptor set
                // "front()" because we just have a single descriptor set that we want to allocate
                // auto tmp = _device->allocateDescriptorSets( setAllocInfo );
                try
                {
                    _frames[i].globalDescriptorSet = _device->allocateDescriptorSets( setAllocInfo )[0];
                } ENGINE_CATCH
            }
            {
                vk::DescriptorSetAllocateInfo setAllocInfo {};
                setAllocInfo.setDescriptorPool( _descriptorPool );
                setAllocInfo.setSetLayouts( _objectSetLayout );
                setAllocInfo.setDescriptorSetCount( 1 );
                try
                {
                    _frames[i].objectDescriptorSet = _device->allocateDescriptorSets( setAllocInfo ).front();
                } ENGINE_CATCH
            }
        }

        {
            /**
             * @brief Information about the buffer that we want to point at in the descriptor
             */
            std::vector<vk::WriteDescriptorSet> setWrite;
            vk::DeviceSize offset = vk::DeviceSize{ 0U };
            /**
             * @brief Write each descriptor to each buffer info
             */
            vk::DescriptorBufferInfo camBuffInfo {};
            vk::DescriptorBufferInfo sceneBuffInfo {};
            vk::DescriptorBufferInfo objectBuffInfo {};
            vk::WriteDescriptorSet camDescSetBuff {};
            vk::WriteDescriptorSet sceneDescSetBuff {};
            vk::WriteDescriptorSet objectDescSetBuff {};
        {
            {
                    camBuffInfo.setBuffer( _frames[i].cameraBuffer.buffer );
                    camBuffInfo.setOffset( offset );    // at offset 0
                    camBuffInfo.setRange( sizeof( GpuCameraData ) ); // this struct is used for uniform buffer (in vertex)

                    camDescSetBuff.setDstSet( _frames[i].globalDescriptorSet );
                    camDescSetBuff.setDstBinding( 0 );
                    camDescSetBuff.setDescriptorType( vk::DescriptorType::eUniformBuffer );
                    camDescSetBuff.setBufferInfo( camBuffInfo );
                    setWrite.emplace_back( camDescSetBuff );
                }
                {
                    sceneBuffInfo.setBuffer( _sceneParameter.allocationBuffer.buffer );
                    sceneBuffInfo.setOffset( offset );  // the start of read the data
                    sceneBuffInfo.setRange( sizeof( GpuSceneParameterData ) ); // this struct is used for uniform buffer (in vertex)

                    sceneDescSetBuff = vk::WriteDescriptorSet {
                        _frames[i].globalDescriptorSet,
                        1,  // binding
                        0,
                        vk::DescriptorType::eUniformBufferDynamic,
                        nullptr,
                        sceneBuffInfo,
                        nullptr
                    };
                    setWrite.push_back( sceneDescSetBuff );
                }
                {
                    objectBuffInfo.setBuffer( _frames[i].objectBuffer.buffer );
                    objectBuffInfo.setOffset( offset );
                    objectBuffInfo.setRange( sizeof( GpuObjectData ) * maxObjectCount );

                    objectDescSetBuff = vk::WriteDescriptorSet {
                        _frames[i].objectDescriptorSet,
                        0,
                        0,
                        vk::DescriptorType::eStorageBuffer,
                        nullptr,
                        objectBuffInfo,
                        nullptr
                    };
                    setWrite.push_back( objectDescSetBuff );
                }
            }

            // updating the descriptor set
            _device->updateDescriptorSets( setWrite, nullptr );
        }
    }
}

FrameData& Engine::getCurrentFrame() 
{
    _frames[ _frameNumber % FRAME_OVERLAP ];
}

size_t Engine::padUniformBufferSize(size_t originalSize) 
{
    // thanks to sascha willems snippet
    size_t minUboAlignment = _physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

void Engine::immediateSubmit(std::function<void( vk::CommandBuffer )>&& func) 
{
    vk::CommandBuffer cmdBuffer;
    {
        vk::CommandBufferAllocateInfo cmdAllocInfo {};
        cmdAllocInfo.setCommandBufferCount( 1 );
        cmdAllocInfo.setCommandPool( _uploadContext.commandPool );
        cmdAllocInfo.setLevel( vk::CommandBufferLevel::ePrimary );

        try
        {
            cmdBuffer = _device->allocateCommandBuffers( cmdAllocInfo ).front(); // 'front()' because just one cmdbuffer
        } ENGINE_CATCH
    }

    vk::CommandBufferBeginInfo beginInfo {};
    beginInfo.setFlags( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );

    try
    {
        cmdBuffer.begin( beginInfo );

        // start executing
        func( cmdBuffer );

        cmdBuffer.end();
    } ENGINE_CATCH

    // Actually there are a lot of information of this submit info that could be filled,
    // but we just need to fill cmdBuffer, the rest are just leave to default initialization.
    vk::SubmitInfo submitInfo {};
    submitInfo.setCommandBuffers( cmdBuffer );

    /**
     * @brief submit command buffer to the queue and then submit it
     * uploadFence will do blocking until the graphics command finish the execution
     */
    _graphicsQueue.submit( submitInfo, _uploadContext.uploadFence );
    _device->waitForFences( _uploadContext.uploadFence, VK_TRUE, UINT64_MAX );
    _device->resetFences( _uploadContext.uploadFence );

    /**
     * @brief Clearing/Freeing command pool, this will also dealocate the command buffer(s) too.
     */
    _device->resetCommandPool( _uploadContext.commandPool );
}

AllocatedImage Engine::loadImageFromFile(const char* filename) 
{
    int texWidth, texHeight, texChannels;

    // STBI_rbg_alpha are exactly equal to eR8G8B8A8Srgb in vulkan
    auto desiredChannel = STBI_rgb_alpha;
    vk::Format imageFormat = vk::Format::eR8G8B8A8Srgb;

    stbi_uc* pixels = stbi_load( filename, &texWidth, &texHeight, &texChannels, desiredChannel );

    if( !pixels )
        throw std::runtime_error( "Failed to load image from file" );

    vk::DeviceSize imageSize = texWidth * texHeight * 4;    // 4 is the sizeof(float)

    AllocatedBuffer staggingBuffer = AllocatedBuffer::createBuffer(
        imageSize, vk::BufferUsageFlagBits::eTransferSrc, _allocator, vma::MemoryUsage::eCpuOnly
    );
    {
        void* pPixels = pixels;
        void* data = _allocator.mapMemory( staggingBuffer.allocation );
        memcpy( data, pPixels, static_cast<size_t>( imageSize ) );
    }
    // after copying the content (in this case, the content is image pixels) to the allocated memory, we no longer use this pixels
    stbi_image_free( pixels ); 

    vk::Extent3D imageExtent;
    imageExtent.setHeight( texHeight );
    imageExtent.setWidth( texWidth );
    imageExtent.setDepth( 1 );

    vk::ImageCreateInfo imageInfo = init::image::initImageInfo(
        imageFormat,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        imageExtent
    );

    vma::AllocationCreateInfo imageAlloc {};
    imageAlloc.setUsage( vma::MemoryUsage::eGpuOnly );

    AllocatedImage image;
    {
        auto tmp = _allocator.createImage( imageInfo, imageAlloc );
        image.image = tmp.first;
        image.allocation = tmp.second;
    }

    immediateSubmit(
        [srcBuff = staggingBuffer.buffer, img = image.image, extent = imageExtent]( vk::CommandBuffer cmdBuffer ){
            vk::ImageSubresourceRange imageSubresource {};
            imageSubresource.setAspectMask( vk::ImageAspectFlagBits::eColor );
            imageSubresource.setLayerCount( 1 );
            imageSubresource.setBaseArrayLayer( 0 );
            imageSubresource.setLevelCount( 1 );
            imageSubresource.setBaseMipLevel( 0 );

            vk::ImageMemoryBarrier imageBarrier2Transfer {};
            imageBarrier2Transfer.setSubresourceRange( imageSubresource );
            imageBarrier2Transfer.setOldLayout( vk::ImageLayout::eUndefined );
            imageBarrier2Transfer.setNewLayout( vk::ImageLayout::eTransferDstOptimal );
            imageBarrier2Transfer.setImage( img );
            imageBarrier2Transfer.setSrcAccessMask( vk::AccessFlags{ 0 } );
            imageBarrier2Transfer.setDstAccessMask( vk::AccessFlagBits::eTransferWrite );

            cmdBuffer.pipelineBarrier( 
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer,
                vk::DependencyFlags{ 0 },
                nullptr,
                nullptr,
                imageBarrier2Transfer
            );

            vk::ImageSubresourceLayers subresource {};
            subresource.setAspectMask( vk::ImageAspectFlagBits::eColor );
            subresource.setMipLevel( 0 );
            subresource.setBaseArrayLayer( 0 );
            subresource.setLayerCount( 1 );

            vk::BufferImageCopy buffer2ImageCopy {};
            // the buffer
            buffer2ImageCopy.setBufferOffset( vk::DeviceSize{ 0 } );
            buffer2ImageCopy.setBufferImageHeight( 0 );
            buffer2ImageCopy.setBufferRowLength( 0 );
            // the image
            buffer2ImageCopy.setImageOffset( vk::DeviceSize{ 0 } );
            buffer2ImageCopy.setImageExtent( extent );
            buffer2ImageCopy.setImageSubresource( subresource );

            cmdBuffer.copyBufferToImage( srcBuff, img, vk::ImageLayout::eTransferDstOptimal, buffer2ImageCopy );

            vk::ImageMemoryBarrier imageBarrier2Read = imageBarrier2Transfer;
            imageBarrier2Read.setOldLayout( vk::ImageLayout::eTransferDstOptimal );
            imageBarrier2Read.setNewLayout( vk::ImageLayout::eShaderReadOnlyOptimal );
            imageBarrier2Read.setSrcAccessMask( vk::AccessFlagBits::eTransferWrite );
            imageBarrier2Read.setDstAccessMask( vk::AccessFlagBits::eShaderRead );

            cmdBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlags{ 0 },
                nullptr,
                nullptr,
                imageBarrier2Read
            );
        }
    );

    _mainDeletionQueue.pushFunction(
        [a = _allocator, img = image](){
            a.destroyImage( img.image, img.allocation );
        }
    );

    _allocator.destroyBuffer( staggingBuffer.buffer, staggingBuffer.allocation );

    return image;
}