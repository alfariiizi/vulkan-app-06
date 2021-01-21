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

    void pushRenderableObject( RenderObject renderObject )
    {
        renderable.emplace_back( renderObject );
    }

    void createMaterial( vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string& name )
    {
        materials[name] = { layout, pipeline };
    }
    void createMaterial( Material material, const std::string& name )
    {
        materials[name] = material;
    }

    void createMesh( Mesh mesh, const std::string& name )
    {
        meshes[name] = mesh;
    }

    Material* getPMaterial( const std::string& name )
    {
        auto it = materials.find( name );
        if( it == materials.end() )
            return nullptr;
        
        return &it->second;
    }

    Mesh* getPMehs( const std::string& name )
    {
        auto it = meshes.find( name );
        if( it == meshes.end() )
            return nullptr;
        
        return &it->second;
    }

    void drawObject( vk::CommandBuffer cmd )
    {
        // camera view
        glm::vec3 camPos = { 0.0f, -6.0f, -10.0f };
        glm::mat4 view = glm::translate( glm::mat4{ 1.0f }, camPos );
        glm::mat4 projection = glm::perspective( glm::radians(70.0f), 1700.0f/ 900.0f, 0.1f, 200.0f );
        projection[1][1] *= -1;

        Mesh* lastMesh = nullptr;
        Material* pLastMaterial = nullptr;

        for( auto& object : renderable )
        {
            // just bind if the materials is valid
            if( object.pMaterial != pLastMaterial )
            {
                cmd.bindPipeline( vk::PipelineBindPoint::eGraphics, object.pMaterial->pipeline );
            }

            glm::mat4 model = object.transformMatrix;

            MeshPushConstant pushconstant;
            // final render matrix that will be calculated on the cpu
            pushconstant.renderMatrix = projection * view * model;
            vk::DeviceSize offsetMaterial = 0;
            cmd.pushConstants<MeshPushConstant>( object.pMaterial->layout, vk::ShaderStageFlagBits::eVertex, offsetMaterial, pushconstant );

            // just bind if the mesh is valid
            if( object.pMesh != lastMesh )
            {
                vk::DeviceSize offsetMesh = 0;
                cmd.bindVertexBuffers( 0, object.pMesh->vertexBuffer.buffer, offsetMesh );
            }

            cmd.draw( object.pMesh->vertices.size(), 1, 0, 0 );
        }
    }
};