
#version 450
#extension GL_ARB_separate_shader_objects : enable

// these are for vertex buffer ( must be match with input description when creating pipeline )
layout( location = 0 ) in vec3 v3Position;
layout( location = 1 ) in vec3 v3Normal;
layout( location = 2 ) in vec3 v3Color;

// this is for the color that passed to fragment shader
layout( location = 0 ) out vec3 fragColor;

// this struct is for buffer that bound in Descriptor set
layout( set = 0, binding = 0 ) uniform CameraBuffer
{
    mat4 view;
    mat4 projection;
    mat4 viewproj;
} cameraData;

// this struct is for push constant ( must be match with 'push contant range' when creating pipeline layout )
layout( push_constant ) uniform constants
{
    vec4 data;
    mat4 renderMatrix;
} PushConstants;

void main()
{
    mat4 transformMatrix = cameraData.viewproj * PushConstants.renderMatrix;
    gl_Position = transformMatrix * vec4( v3Position, 1.0 );
    fragColor = vec3( v3Color );
}