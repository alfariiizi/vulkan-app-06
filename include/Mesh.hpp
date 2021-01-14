#pragma once

#include "GraphicsPipeline.hpp"
#include "utils.hpp"

#include <iostream>

#ifndef ENGINE_CATCH
#define ENGINE_CATCH                            \
    catch( const vk::SystemError& err )         \
    {                                           \
        throw std::runtime_error( err.what() ); \
    }
#endif

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;

    static VertexInputDescription getVertexInputDescription() 
    {
        VertexInputDescription description;

        // we just have 1 binding
        vk::VertexInputBindingDescription mainBinding{};
        mainBinding.setBinding( 0 );
        mainBinding.setInputRate( vk::VertexInputRate::eVertex );
        mainBinding.setStride( sizeof( Vertex ) );
        description.bindings.push_back( mainBinding );


        // position will be stored at location = 0
        vk::VertexInputAttributeDescription positionAttribute {};
        positionAttribute.setBinding( 0 );
        positionAttribute.setLocation( 0 );
        positionAttribute.setFormat( vk::Format::eR32G32B32Sfloat );
        positionAttribute.setOffset( offsetof( Vertex, position ) );
        description.attributs.push_back( positionAttribute );

        // normal will be stored at location = 1
        vk::VertexInputAttributeDescription normalAttribute {};
        normalAttribute.setBinding( 0 );
        normalAttribute.setLocation( 1 );
        normalAttribute.setFormat( vk::Format::eR32G32B32Sfloat );
        normalAttribute.setOffset( offsetof( Vertex, normal ) );
        description.attributs.push_back( normalAttribute );

        // color will be stored at location = 2
        vk::VertexInputAttributeDescription colorAttribute {};
        colorAttribute.setBinding( 0 );
        colorAttribute.setLocation( 2 );
        colorAttribute.setFormat( vk::Format::eR32G32B32Sfloat );
        colorAttribute.setOffset( offsetof( Vertex, color ) );
        description.attributs.push_back( colorAttribute );


        return description;
    }
};

struct Mesh
{
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    bool loadFromObj( const std::string& filename );
};

class MeshLoaderGraphicsPipeline : public GraphicsPipeline
{
public:
    Mesh getMesh() const;
    virtual void load( const std::string& objFilename );
    void setAllocator( const vma::Allocator& allocator );

public:
    void setMesh( Mesh mesh );
    virtual void createPipelineLayout() override
    {
        m_pushConstant.setSize( sizeof(MeshPushConstant) );
        m_pushConstant.setOffset( 0 );
        m_pushConstant.setStageFlags( vk::ShaderStageFlagBits::eVertex );

        m_pipelineLayoutInfo.setSetLayouts( nullptr );
        m_pipelineLayoutInfo.setPushConstantRanges( m_pushConstant );

        try
        {
            m_pipelineLayout = device.createPipelineLayout( m_pipelineLayoutInfo );
        } ENGINE_CATCH

        m_graphicsPipelineInfo.setLayout( m_pipelineLayout );
    }
    virtual void upload();

private:
    Mesh m_mesh;
    vma::Allocator m_allocator;
};
