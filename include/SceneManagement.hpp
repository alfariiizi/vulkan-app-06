#pragma once

#include <vulkan/vulkan.hpp>

#include "Mesh.hpp"


struct Material
{
    vk::DescriptorSet textureSet = nullptr; // descriptor set for texturing
    vk::PipelineLayout layout;
    vk::Pipeline pipeline;
};

struct Texture
{
    AllocatedImage image;
    vk::ImageView imageView;
};

struct RenderObject
{
    Mesh* pMesh;
    Material* pMaterial;
    Texture* pTexture;
    glm::mat4 transformMatrix;
};

struct SceneManagement
{
    std::vector<RenderObject> renderable;
    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;

    void pushRenderableObject( RenderObject renderObject );

    void createMaterial( vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string& name, vk::DescriptorSet dscSet = nullptr );
    // void createMaterial( Material material, const std::string& name );

    void createMesh( Mesh mesh, const std::string& name );

    void createTexture( Texture texture, const std::string& name );

    Material* getPMaterial( const std::string& name );
    Mesh* getPMehs( const std::string& name );
    Texture* getPTexture( const std::string& name );

    void drawObject( vk::CommandBuffer cmd, FrameData currentFrame, const uint32_t descOffset );
};