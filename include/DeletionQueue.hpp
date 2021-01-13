#pragma once

#include <functional>
#include <deque>

class DeletionQueue
{
public:
    void pushFunction( const std::function<void()>& function )
    {
        deletors.push_back( function );
    }
    void flush()
    {
        for( auto it = deletors.rbegin(); it != deletors.rend(); ++it )
        {
            (*it)();    // call the function
        }
        deletors.clear();
    }

private:
    std::deque<std::function<void()>> deletors;
};