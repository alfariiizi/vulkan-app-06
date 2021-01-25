#include "utils.hpp"

#define VMA_IMPLEMENTATION

AllocatedBuffer AllocatedBuffer::createBuffer( size_t allocSize, vk::BufferUsageFlags bufferUsage, vma::Allocator allocator, vma::MemoryUsage memoryUsage )
{
    vk::BufferCreateInfo bufferInfo {};
    bufferInfo.setSize( vk::DeviceSize{ allocSize } );
    bufferInfo.setUsage( bufferUsage );

    vma::AllocationCreateInfo allocInfo {};
    allocInfo.setUsage( memoryUsage );

    AllocatedBuffer newbuffer;
    auto temp = allocator.createBuffer( bufferInfo, allocInfo );
    newbuffer.buffer = temp.first;
    newbuffer.allocation = temp.second;

    return newbuffer;
}