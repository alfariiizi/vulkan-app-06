#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "vk_mem_alloc.hpp"

#include "DeletionQueue.hpp"
#include "Mesh.hpp"
#include "SceneManagement.hpp"

#define FRAME_OVERLAP 2

class Engine
{
public:
    Engine();
    ~Engine();

private:
    void run();
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanUp();

private:
    void createMainVulkanComponent();
    void createSwapchainComponent();
    void createCommandComponent();

private:
    void createRenderPass();
    void createFramebuffers();
    void createSyncObject();

private:
    void createMemoryAllocator();

private:
    void beginFrame();  // begin to wait and reset the fence
    void draw( vk::CommandBuffer cmd );
    void record();      // recording
    void endFrame();    // executing the command

private:
    void uploadMesh( Mesh& mesh );

private:
    void createObjectToRender();
    void createMaterials();
    void createMeshes();
    void initRenderObject();

private:
    void createTriangleMesh();
    void createMonkeyMesh();

private:
    void defaultMaterial();
    void colorMaterial();

private:
    void initDescriptors();

private:
    FrameData& getCurrentFrame();
    size_t padUniformBufferSize( size_t originalSize );

private:
    vk::DescriptorSetLayout _globalSetLayout;
    vk::DescriptorSetLayout _objectSetLayout;
    vk::DescriptorPool _descriptorPool;
    SceneParameter _sceneParameter;

private:
    SceneManagement _sceneManag;

private:
    std::array<FrameData, FRAME_OVERLAP> _frames;

public:
    static constexpr unsigned int ScreenWidth       = 800U;
    static constexpr unsigned int ScreenHeight      = 600U;

private:
    GLFWwindow      * _window;
    vk::SurfaceKHR   _surface;

private:
    vk::UniqueInstance          _instance;
    VkDebugUtilsMessengerEXT    _debugUtilsMessenger;
    DeletionQueue               _mainDeletionQueue;
    vk::PhysicalDevice          _physicalDevice;
    vk::PhysicalDeviceProperties _physicalDeviceProperties;
    vk::UniqueDevice            _device;
    vk::Queue _graphicsQueue;
    vk::Queue _presentQueue;

private:
    vma::Allocator _allocator;

private:
    vk::SwapchainKHR                _swapchain;
    vk::Format                      _swapchainFormat;
    vk::Extent2D                    _swapchainExtent;
    // these images are not created from the device, but these are from the swapchain. It'll be destroyed when swapchain get destroyed
    std::vector<vk::Image>          _swapchainImages;
    // these image views are created from the device to do the "view" job to swapchain images
    std::vector<vk::ImageView>      _swapchainImageViews;

private:
    vk::RenderPass                  _renderPass;
    std::vector<vk::Framebuffer>    _swapchainFramebuffers;
    AllocatedImage                  _depthImage;
    vk::ImageView                   _depthImageView;
    vk::Format                      _depthFormat;

private:
    // vk::PipelineLayout      _pipelineLayout;
    // vk::Pipeline            _graphicsPipeline;
    uint32_t                _frameNumber = 0;

private:
    // vk::UniqueCommandPool                   _commandPool;
    // // std::vector<vk::UniqueCommandBuffer>    _commandBuffers;
    // vk::UniqueCommandBuffer                 _mainCommandBuffer;

private:
    // vk::UniqueSemaphore  _renderSemaphore;
    // vk::UniqueSemaphore  _presentSemaphore;
    // vk::UniqueFence      _renderFence;
    size_t               _currentFrame                 = 0;
    unsigned long        _timeOut                      = 1000000000;   // 1 second = 10^(9) nano second
    uint32_t             _imageIndex                   = 0;

};