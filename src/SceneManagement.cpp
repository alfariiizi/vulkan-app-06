#include "SceneManagement.hpp"

void SceneManagement::pushRenderableObject( RenderObject renderObject )
{
    renderable.emplace_back( renderObject );
}

void SceneManagement::createMaterial( vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string& name )
{
    materials[name] = { layout, pipeline };
}

void SceneManagement::createMaterial( Material material, const std::string& name )
{
    materials[name] = material;
}

void SceneManagement::createMesh( Mesh mesh, const std::string& name )
{
    meshes[name] = mesh;
}

void SceneManagement::createTexture(Texture texture, const std::string& name) 
{
    textures[name] = texture;
}

Material* SceneManagement::getPMaterial( const std::string& name )
{
    auto it = materials.find( name );
    if( it == materials.end() )
        return nullptr;
    
    return &it->second;
}

Mesh* SceneManagement::getPMehs( const std::string& name )
{
    auto it = meshes.find( name );
    if( it == meshes.end() )
        return nullptr;
    
    return &it->second;
}

void SceneManagement::drawObject( vk::CommandBuffer cmd, FrameData currentFrame, const uint32_t descOffset )
{
    Mesh* lastMesh = nullptr;
    Material* pLastMaterial = nullptr;

    for( auto& object : renderable )
    {
        // just bind if the materials is valid
        if( object.pMaterial != pLastMaterial )
        {
            cmd.bindPipeline( vk::PipelineBindPoint::eGraphics, object.pMaterial->pipeline );

            // cmd.bindDescriptorSets( 
            //     vk::PipelineBindPoint::eGraphics, object.pMaterial->layout, 
            //     0,                                  // first set
            //     currentFrame.globalDescriptorSet,      // descriptor set (this could be an array)
            //     nullptr                             // dynamic offset
            // );
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                object.pMaterial->layout,
                0,
                currentFrame.globalDescriptorSet,
                descOffset
            );
        }

        // glm::mat4 model = object.transformMatrix;

        MeshPushConstant pushconstant;
        // final render matrix that will be calculated on the cpu
        // pushconstant.renderMatrix = projection * view * model;
        pushconstant.renderMatrix = object.transformMatrix;
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