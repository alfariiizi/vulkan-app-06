#include "Vulkan_Init.hpp"

#ifndef ENGINE_CATCH
#define ENGINE_CATCH                            \
    catch( const vk::SystemError& err )         \
    {                                           \
        throw std::runtime_error( err.what() ); \
    }
#endif

vk::UniqueInstance init::createInstance()
{
    auto appInfo = vk::ApplicationInfo{
        "Vulkan Engine",
        VK_MAKE_VERSION( 1, 0, 0 ),
        "Vulkan Engine",
        VK_MAKE_VERSION( 1, 0, 0 ),
        VK_API_VERSION_1_1
    };

    /**
     * The Extensension
     */
    auto instanceExtensions = vk::enumerateInstanceExtensionProperties();
    auto enabledExtensions = utils::getRequiredExtensions();
    for ( const auto& extension : enabledExtensions )
    {
        auto found = std::find_if( instanceExtensions.begin(), instanceExtensions.end(),
                        [&extension]( const vk::ExtensionProperties& ext ){ return strcmp( ext.extensionName, extension ) == 0; }
        );
        assert( found != instanceExtensions.end() );
    }

    /**
     * The Validation Layers
     */
    auto instanceLayers = vk::enumerateInstanceLayerProperties();
    auto validationLayers = utils::getValidationLayers();
    std::vector<const char*> enableValidationLayers;
    enableValidationLayers.reserve( validationLayers.size() );
    for( const auto& layer : validationLayers )
    {
        auto found = std::find_if( instanceLayers.begin(), instanceLayers.end(),
                            [&layer]( const vk::LayerProperties& l ){ return strcmp( l.layerName, layer ); }
        );
        assert( found != instanceLayers.end() );
        enableValidationLayers.push_back( layer );
    }

    vk::InstanceCreateInfo instanceInfo {
        vk::InstanceCreateFlags(), &appInfo, enableValidationLayers, enabledExtensions 
    };

    auto severityFlags = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    auto messegeTypeFlags = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    vk::DebugUtilsMessengerCreateInfoEXT debugutilsMessengerInfo{
        vk::DebugUtilsMessengerCreateFlagsEXT(),
        severityFlags,
        messegeTypeFlags,
        utils::debugUtilsMessengerCallback
    };
    instanceInfo.setPNext( reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>( &debugutilsMessengerInfo ) );

    try
    {
        return vk::createInstanceUnique( instanceInfo );
    } ENGINE_CATCH
}

void init::createDebugUtilsMessengerInfo( const vk::Instance& instance, VkDebugUtilsMessengerEXT& outDebugUtilsMessenger ) 
{
    auto severityFlags = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    auto messegeTypeFlags = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerInfo {
        vk::DebugUtilsMessengerCreateFlagsEXT(),
        severityFlags,
        messegeTypeFlags,
        utils::debugUtilsMessengerCallback
    };

    if( utils::CreateDebugUtilsMessengerEXT( 
            instance, 
            reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>( &debugUtilsMessengerInfo ), 
            nullptr, 
            &outDebugUtilsMessenger )
        != VK_SUCCESS )
    {
        throw std::runtime_error( "FAILED: Creating Debug Utils Messenger" );
    }
}

vk::SurfaceKHR init::createSurfce( const vk::Instance& instance, GLFWwindow* window )
{
    VkSurfaceKHR surface;
    auto result = glfwCreateWindowSurface( instance, window, nullptr, &surface );
    if( result != VK_SUCCESS )
        throw std::runtime_error( "FAILED: Creating Window Surface" );
    
    return vk::SurfaceKHR( surface );
}

vk::PhysicalDevice init::pickPhysicalDevice( const vk::Instance& instance, const vk::SurfaceKHR& surface )
{
    auto physicalDevices = instance.enumeratePhysicalDevices();
    vk::PhysicalDevice choose;
    for( auto& physicalDevice : physicalDevices )
    {
        if( utils::IsDeviceSuitable( physicalDevice, surface ) )
        {
            choose = physicalDevice;
            break;
        }
    }

    return choose;
}

vk::UniqueDevice init::createDevice( const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface )
{
    auto graphicsAndPresentQueueFamily = utils::FindQueueFamilyIndices( physicalDevice, surface );

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo {
        vk::DeviceQueueCreateFlags(),
        graphicsAndPresentQueueFamily.graphicsFamily.value(),
        1,              // queue count
        &queuePriority  // queue priority
    };

    auto deviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    std::vector<const char*> enabledExtension = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    std::vector<const char*> extensions; extensions.reserve( enabledExtension.size() );
    for ( const auto& extension : enabledExtension )
    {
        auto found = std::find_if( deviceExtensions.begin(), deviceExtensions.end(),
                        [&extension]( const vk::ExtensionProperties& ext ){ return strcmp( ext.extensionName, extension ) == 0; }
        );
        assert( found != deviceExtensions.end() );
        extensions.push_back( extension );
    }

    auto deviceFeatures = physicalDevice.getFeatures();
    vk::DeviceCreateInfo deviceInfo {
        vk::DeviceCreateFlags(),
        queueInfo,
        utils::getValidationLayers(),   // device validation layers
        extensions,                     // device extensions
        &deviceFeatures                 // device features
    };

    try
    {
        return physicalDevice.createDeviceUnique( deviceInfo );
    } ENGINE_CATCH
}

vk::SwapchainKHR init::sc::createSwapchain( const vk::PhysicalDevice& physicalDevice, const vk::Device& device, const vk::SurfaceKHR& surface )
{
    auto surfaceCapability = physicalDevice.getSurfaceCapabilitiesKHR( surface );
    auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR( surface );
    auto surfacePresentModes = physicalDevice.getSurfacePresentModesKHR( surface );

    auto surfaceExtent = utils::sc::chooseSurfaceExtent( surfaceCapability );
    auto surfaceFormat = utils::sc::chooseSurfaceFormat( surfaceFormats );
    auto presentMode = utils::sc::choosePresentMode( surfacePresentModes );

    uint32_t imageCount = surfaceCapability.minImageCount + 1;
    if (surfaceCapability.maxImageCount > 0 && imageCount > surfaceCapability.maxImageCount) {
        imageCount = surfaceCapability.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR swapchainInfo {
        {},
        surface,
        imageCount,
        surfaceFormat.format,       // surfaceFormat.format == image format
        surfaceFormat.colorSpace,   // surfaceFormat.colorspace == image colorspace
        surfaceExtent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        {},  // sharing mode
        {},  // queue family indices
        surfaceCapability.currentTransform,     // transform
        vk::CompositeAlphaFlagBitsKHR::eOpaque, // composite alpha
        presentMode,
        VK_TRUE,                                // clipped
        nullptr                                 // old swapchain
    };

    auto graphicsAndPresentQueueFamily = utils::FindQueueFamilyIndices( physicalDevice, surface );
    if( !graphicsAndPresentQueueFamily.exslusive() )
    {
        swapchainInfo.setImageSharingMode( vk::SharingMode::eConcurrent );
        swapchainInfo.setQueueFamilyIndices( graphicsAndPresentQueueFamily.graphicsAndPresentFamilyIndex() );
    }
    else
    {
        swapchainInfo.setImageSharingMode( vk::SharingMode::eExclusive );
        swapchainInfo.setQueueFamilyIndices( nullptr );
    }
    
    
    try
    {
        return device.createSwapchainKHR( swapchainInfo );
    } ENGINE_CATCH
}

void init::sc::retrieveImagesAndCreateImageViews( const vk::Device& device, const vk::SwapchainKHR& swapchain, vk::Format swapchainFormat,std::vector<vk::Image>& outSwapchainImages, std::vector<vk::ImageView>& outSwapchainImageViews )
{
    outSwapchainImages = device.getSwapchainImagesKHR( swapchain );
    outSwapchainImageViews.reserve( outSwapchainImages.size() );

    for( auto& image : outSwapchainImages )
    {
        vk::ImageViewCreateInfo imageViewInfo {
            vk::ImageViewCreateFlags(),
            image,
            vk::ImageViewType::e2D,
            swapchainFormat,
            vk::ComponentMapping{
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
            },
            vk::ImageSubresourceRange {
                vk::ImageAspectFlagBits::eColor,    // aspect flags
                0,      // base mip map
                1,      // level count
                0,      // base array layer
                1       // layer count
            }
        };
        try
        {
            outSwapchainImageViews.push_back( device.createImageView( imageViewInfo ) );
        } ENGINE_CATCH
    }
}

vk::UniqueCommandPool init::cm::createCommandPool( const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const vk::Device& device )
{
    auto graphicsAndPresentQueueFamilyIndex = utils::FindQueueFamilyIndices( physicalDevice, surface );

    vk::CommandPoolCreateInfo cmdPoolInfo {};
    cmdPoolInfo.setFlags( vk::CommandPoolCreateFlagBits::eResetCommandBuffer );
    cmdPoolInfo.setQueueFamilyIndex( graphicsAndPresentQueueFamilyIndex.graphicsFamily.value() );

    try
    {
        return device.createCommandPoolUnique( cmdPoolInfo );
    } ENGINE_CATCH
}

std::vector<vk::UniqueCommandBuffer> init::cm::createCommandBuffers( const vk::Device& device, const vk::CommandPool& cmdPool, vk::CommandBufferLevel level, uint32_t count )
{
    vk::CommandBufferAllocateInfo cmdBufferAllocInfo {};
    cmdBufferAllocInfo.setCommandPool( cmdPool );
    cmdBufferAllocInfo.setLevel( level );
    cmdBufferAllocInfo.setCommandBufferCount( count );  // just 1 command buffer

    try
    {
        return device.allocateCommandBuffersUnique( cmdBufferAllocInfo );
    } ENGINE_CATCH
}