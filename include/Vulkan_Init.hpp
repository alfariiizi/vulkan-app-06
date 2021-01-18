#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "Initializer.hpp"

namespace init
{

vk::UniqueInstance  createInstance                  ();
void                createDebugUtilsMessengerInfo   ( const vk::Instance& instance, VkDebugUtilsMessengerEXT& outDebugUtilsMessenger );
vk::SurfaceKHR      createSurfce                    ( const vk::Instance& instance, GLFWwindow* window );
vk::PhysicalDevice  pickPhysicalDevice              ( const vk::Instance& instance, const vk::SurfaceKHR& surface );
vk::UniqueDevice    createDevice                    ( const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface );

namespace sc // swapchain
{
vk::SwapchainKHR    createSwapchain( const vk::PhysicalDevice& physicalDevice, const vk::Device& device, const vk::SurfaceKHR& surface );
void retrieveImagesAndCreateImageViews( const vk::Device& device, const vk::SwapchainKHR& swapchain, vk::Format swapchainFormat, std::vector<vk::Image>& outSwapchainImages, std::vector<vk::ImageView>& outSwapchainImageViews );
} // namespace sc

namespace cm // command
{
vk::UniqueCommandPool createCommandPool( const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const vk::Device& device );
std::vector<vk::UniqueCommandBuffer> createCommandBuffers( const vk::Device& device, const vk::CommandPool& cmdPool, vk::CommandBufferLevel level, uint32_t count );
} // namespace cm

namespace gp // graphics pipeline
{
// coming soon (maybe)
} // namespace gp

namespace image
{
vk::ImageCreateInfo initImageInfo( vk::Format format, vk::ImageUsageFlags usageFlag, vk::Extent3D extent );
vk::ImageViewCreateInfo initImageViewInfo( vk::Format format, vk::Image theImage, vk::ImageAspectFlags aspectMask );
}

};
