#pragma once

#include <vulkan/vulkan.hpp>

#include "Mesh.hpp"


struct Material
{
    vk::PipelineLayout layout;
    vk::Pipeline pipeline;
};

struct RenderObject
{
    Mesh* pMesh;
    Material* pMaterial;
    glm::mat4 transformMatrix;
};

struct SceneManagement
{
    std::vector<RenderObject> renderable;
    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, Mesh> meshes;

    void pushRenderableObject( RenderObject renderObject );

    void createMaterial( vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string& name );
    void createMaterial( Material material, const std::string& name );

    void createMesh( Mesh mesh, const std::string& name );

    Material* getPMaterial( const std::string& name );
    Mesh* getPMehs( const std::string& name );

    void drawObject( vk::CommandBuffer cmd, FrameData currentFrame, const uint32_t descOffset );
};