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

                    Vertex newVertex;

                    // vertex position
                    {
                        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                        newVertex.position.x = vx;
                        newVertex.position.y = vy;
                        newVertex.position.z = vz;
                    }

                    // vertex normal
                    {
                        tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                        tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                        tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
                        newVertex.normal.x = nx;
                        newVertex.normal.y = ny;
                        newVertex.normal.z = nz;
                    }

                    // vertex color
                    {
                        newVertex.color = newVertex.normal;
                    }

                    // vertex uv
                    {
                        tinyobj::real_t ux = attrib.texcoords[ 2 * idx.texcoord_index + 0 ];
                        tinyobj::real_t uy = attrib.texcoords[ 2 * idx.texcoord_index + 1 ];
                        newVertex.uv.x = ux;
                        newVertex.uv.y = 1 - uy;
                    }

                    this->vertices.emplace_back( newVertex );
                }
            }
        }

        return true;
}