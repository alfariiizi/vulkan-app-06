#include "Initializer.hpp"

#include <iostream>
#include <fstream>

#include <GLFW/glfw3.h>

#include "Engine.hpp"

#ifndef ENGINE_CATCH
#define ENGINE_CATCH                            \
    catch( const vk::SystemError& err )         \
    {                                           \
        throw std::runtime_error( err.what() ); \
    }                                           
#endif

namespace utils
{
VKAPI_ATTR VkBool32 VKAPI_CALL
    debugUtilsMessengerCallback( VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
                                VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
                                VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
                                void * /*pUserData*/ )
{
    std::cerr << vk::to_string( static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( messageSeverity ) ) << ": "
            << vk::to_string( static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( messageTypes ) ) << ":\n";
    std::cerr << "\t"
            << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
    std::cerr << "\t"
            << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    std::cerr << "\t"
            << "message         = <" << pCallbackData->pMessage << ">\n";
    if ( 0 < pCallbackData->queueLabelCount )
    {
    std::cerr << "\t"
                << "Queue Labels:\n";
    for ( uint8_t i = 0; i < pCallbackData->queueLabelCount; i++ )
    {
        std::cerr << "\t\t"
                << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
    }
    }
    if ( 0 < pCallbackData->cmdBufLabelCount )
    {
    std::cerr << "\t"
                << "CommandBuffer Labels:\n";
    for ( uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++ )
    {
        std::cerr << "\t\t"
                << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
    }
    }
    if ( 0 < pCallbackData->objectCount )
    {
    std::cerr << "\t"
                << "Objects:\n";
    for ( uint8_t i = 0; i < pCallbackData->objectCount; i++ )
    {
        std::cerr << "\t\t"
                << "Object " << i << "\n";
        std::cerr << "\t\t\t"
                << "objectType   = "
                << vk::to_string( static_cast<vk::ObjectType>( pCallbackData->pObjects[i].objectType ) ) << "\n";
        std::cerr << "\t\t\t"
                << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
        if ( pCallbackData->pObjects[i].pObjectName )
        {
        std::cerr << "\t\t\t"
                    << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">";
        }
    }

    std::cerr << "\n\n";
    }
    return VK_TRUE;
}

std::vector<const char*> getValidationLayers() 
{
    return { "VK_LAYER_KHRONOS_validation" };
}

std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if( func != nullptr )
    {
        return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
)
{
    auto func = ( PFN_vkDestroyDebugUtilsMessengerEXT )vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
    if( func != nullptr )
        func( instance, debugMessenger, pAllocator );
}

bool IsDeviceSuitable(  const vk::PhysicalDevice& physicalDevice,
                        const vk::SurfaceKHR& surface )
{
    auto graphicsAndPresentQueueFamily = FindQueueFamilyIndices( physicalDevice, surface );
    return graphicsAndPresentQueueFamily.hasValue();
}

QueueFamilyIndices FindQueueFamilyIndices(  const vk::PhysicalDevice& physicalDevice,
                                            const vk::SurfaceKHR& surface )
{
    QueueFamilyIndices queueFamilyIndices;

    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    size_t i = 0;
    for( const auto& q : queueFamilies )
    {
        // if graphics and present queue family are the same index
        if( q.queueCount > 0 
            && q.queueFlags & vk::QueueFlagBits::eGraphics
            && physicalDevice.getSurfaceSupportKHR( i, surface )
        )
        {
            queueFamilyIndices.graphicsFamily = i;
            queueFamilyIndices.presentFamily = i;
            break;
        }

        // if graphics and present family are not the same index
        else if( q.queueCount > 0
            && q.queueFlags & vk::QueueFlagBits::eGraphics )
        {
            queueFamilyIndices.graphicsFamily = i;
        }
        else if( physicalDevice.getSurfaceSupportKHR( i, surface ) )
        {
            queueFamilyIndices.presentFamily = i;
        }
        
        // if both graphics and present has been initialize
        if( queueFamilyIndices.hasValue() )
            break;
        ++i;
    }

    // if cannot find graphics and/or queue family that support vulkan
    if( !queueFamilyIndices.hasValue() )
    {
        throw std::runtime_error( "FAILED: Find Graphics and/or Queue Family Indices" );
    }

    return queueFamilyIndices;
}

} // namespace utils


namespace utils
{
    namespace sc
    {
        vk::Extent2D chooseSurfaceExtent(vk::SurfaceCapabilitiesKHR& surfaceCapabilities) 
        {
            if( surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() )
                return surfaceCapabilities.currentExtent;
            else
            {
                vk::Extent2D actualExtent = { static_cast<uint32_t>( Engine::ScreenWidth ), 
                                                static_cast<uint32_t>( Engine::ScreenHeight ) };
                
                actualExtent.width = std::clamp( actualExtent.width, 
                                                    surfaceCapabilities.minImageExtent.width, 
                                                    surfaceCapabilities.maxImageExtent.width );
                
                actualExtent.height = std::clamp( actualExtent.height, 
                                                    surfaceCapabilities.minImageExtent.height, 
                                                    surfaceCapabilities.maxImageExtent.height );
                
                return actualExtent;
            }
        }
        
        vk::SurfaceFormatKHR chooseSurfaceFormat(std::vector<vk::SurfaceFormatKHR>& surfaceFormats) 
        {
            // if just found one surface format
            if( surfaceFormats.size() == 1 && surfaceFormats[0].format == vk::Format::eUndefined )
            {
                return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
            }

            // if found more than one surface format
            for( auto& surfaceFormat : surfaceFormats )
            {
                if( surfaceFormat.format == vk::Format::eB8G8R8A8Unorm 
                    && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear )
                {
                    return surfaceFormat;
                }
            }

            // otherwise, just choose the avalible surface format
            return surfaceFormats[0];
        }

        vk::PresentModeKHR choosePresentMode(std::vector<vk::PresentModeKHR>& presentModes) 
        {
            // choose the present mode
            for( auto& presentMode : presentModes )
            {
                if( presentMode == vk::PresentModeKHR::eFifo )
                {
                    return presentMode;
                }
            }

            // otherwise, just choose the available present mode
            return presentModes[0];
        }
    }

    namespace gp
    {
        std::vector<char> readFile( const std::string& fileName )
        {
            std::ifstream file(fileName, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("FAILED TO OPEN SHADER FILE\n");
            }

            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();

            return buffer;
        }

        vk::UniqueShaderModule createShaderModule( const vk::Device& device, const std::vector<char>& code )
        {
            try
            {
                return device.createShaderModuleUnique( 
                    vk::ShaderModuleCreateInfo {
                        vk::ShaderModuleCreateFlags(),
                        code.size(),
                        reinterpret_cast<const uint32_t*>( code.data() )
                    }
                 );
            } ENGINE_CATCH
        }
    } // namespace gp
        
} // namespace utils