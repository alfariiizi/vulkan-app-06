#include "Mesh.hpp"

#define VMA_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

bool Mesh::loadFromObj(const std::string& filename) 
{
        // attribute will contain vertex array of an object file
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        // error handling message
        std::string warn;
        std::string err;

        // tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &err, filename.c_str(), nullptr );
        auto result = tinyobj::LoadObj( &attrib, &shapes, &materials, &err, filename.c_str(), nullptr );
        if( !result )
        {
            std::cerr << err << '\n';
            return false;
        }
        
        auto sizeShapes = shapes.size();
        for( size_t i = 0; i < sizeShapes; ++i )
        {
            const auto sizeTheFaces = shapes[i].mesh.num_face_vertices.size();

            // hardcorde loading to triangle
            int fv = 3;
            // loop over faces (polygon)
            for( size_t j = 0, indexOffset = 0; j < sizeTheFaces; ++j, indexOffset += fv )
            {
                // loop over vertices
                for( size_t k = 0; k < fv; ++k )
                {
                    // get access to vertex
                    tinyobj::index_t idx = shapes[i].mesh.indices[indexOffset + k];

                    // vertex position
                    tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                    tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                    tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                    // vertex normal
                    tinyobj::real_t nx = attrib.normals[3 * idx.vertex_index + 0];
                    tinyobj::real_t ny = attrib.normals[3 * idx.vertex_index + 1];
                    tinyobj::real_t nz = attrib.normals[3 * idx.vertex_index + 2];

                    Vertex newVertex;
                    newVertex.position.x = vx;
                    newVertex.position.y = vy;
                    newVertex.position.z = vz;
                    newVertex.normal.x = nx;
                    newVertex.normal.y = ny;
                    newVertex.normal.z = nz;
                    newVertex.color = newVertex.normal;

                    this->vertices.emplace_back( newVertex );
                }
            }
        }

        return true;
}

Mesh MeshLoaderGraphicsPipeline::getMesh() const
{
    return m_mesh;
}

void MeshLoaderGraphicsPipeline::load(const std::string& objFilename) 
{
    m_mesh.loadFromObj( objFilename );
    upload();
}

void MeshLoaderGraphicsPipeline::upload() 
{
    size_t size = m_mesh.vertices.size() * sizeof(Vertex);

    vk::BufferCreateInfo bufferInfo {};
    bufferInfo.setSize( size );
    bufferInfo.setUsage( vk::BufferUsageFlagBits::eVertexBuffer );

    vma::AllocationCreateInfo allocInfo {};
    allocInfo.setUsage( vma::MemoryUsage::eCpuToGpu );

    auto buffer = m_allocator.createBuffer( bufferInfo, allocInfo );
    m_mesh.vertexBuffer.buffer = buffer.first;
    m_mesh.vertexBuffer.allocation = buffer.second;
    m_deletionQueue.pushFunction(
        [a = m_allocator, m = m_mesh](){
            a.destroyBuffer( m.vertexBuffer.buffer, m.vertexBuffer.allocation );
        }
    );

    void* data = m_allocator.mapMemory( m_mesh.vertexBuffer.allocation );
    memcpy( data, m_mesh.vertices.data(), size );
    m_allocator.unmapMemory( m_mesh.vertexBuffer.allocation );
}

void MeshLoaderGraphicsPipeline::setMesh(Mesh mesh) 
{
    m_mesh = mesh;
}

void MeshLoaderGraphicsPipeline::setAllocator(const vma::Allocator& allocator) 
{
    m_allocator = allocator;
}