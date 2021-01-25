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

void SceneManagement::drawObject( vk::CommandBuffer cmd, FrameData currentFrame, vma::Allocator allocator )
{
    // camera view
    glm::vec3 camPos = { 0.0f, -6.0f, -10.0f };
    glm::mat4 view = glm::translate( glm::mat4{ 1.0f }, camPos );
    glm::mat4 projection = glm::perspective( glm::radians(70.0f), 1700.0f/ 900.0f, 0.1f, 200.0f );
    projection[1][1] *= -1;

    /**
     * @brief Filling the GPU Camera structure
     */
    GpuCameraData camData;
    camData.projection = projection;
    camData.view = view;
    camData.viewproj = projection * view;

    /**
     * @brief Copy the GPU Camera structure to the buffer
     */
    void* data = allocator.mapMemory( currentFrame.cameraBuffer.allocation );
    memcpy( data, &camData, sizeof(GpuCameraData) );
    allocator.unmapMemory( currentFrame.cameraBuffer.allocation );


    Mesh* lastMesh = nullptr;
    Material* pLastMaterial = nullptr;

    for( auto& object : renderable )
    {
        // just bind if the materials is valid
        if( object.pMaterial != pLastMaterial )
        {
            cmd.bindPipeline( vk::PipelineBindPoint::eGraphics, object.pMaterial->pipeline );
            cmd.bindDescriptorSets( 
                vk::PipelineBindPoint::eGraphics, object.pMaterial->layout, 
                0,                                  // first set
                currentFrame.globalDescriptor,      // descriptor set (this could be an array)
                nullptr                             // dynamic offset
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