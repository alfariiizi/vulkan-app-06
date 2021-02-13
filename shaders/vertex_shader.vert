#version 460
#extension GL_ARB_separate_shader_objects : enable

// these are for vertex buffer ( must be match with input description when creating pipeline )
layout( location = 0 ) in vec3 v3Position;
layout( location = 1 ) in vec3 v3Normal;
layout( location = 2 ) in vec3 v3Color;
layout( location = 3 ) in vec2 v2TexCoord;

// this is for the color that passed to fragment shader
layout( location = 0 ) out vec3 fragColor;
layout( location = 1 ) out vec2 texCoord;

// this struct is for buffer that bound in Descriptor set
layout( set = 0, binding = 0 ) uniform GpuCameraData
{
    mat4 view;
    mat4 projection;
    mat4 viewproj;
} cameraData;

// this struct is used in ObjectBuffer below
struct ObjectData
{
    mat4 model;
};

// std140: is used to match how arrays work in cpp. this enforce some rules about how the memory is laid out, and what is allignment.
// set = 1: is because this struct is used for another descriptor set, not the same descriptor as GpuCameraData.
// binding = 0: is because this is the first binding of the descriptor set that being used.
// readonly: shader storage buffer can be read or write, so we let the shader know what characteristic this data gonna be.
layout( std140, set = 1, binding = 0 ) readonly buffer ObjectBuffer
{
    ObjectData objects [];
} objectBuffer;

// this struct is for push constant ( must be match with 'push contant range' when creating pipeline layout )
layout( push_constant ) uniform constants
{
    vec4 data;
    mat4 renderMatrix;
} PushConstants;

void main()
{
    mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
    mat4 transformMatrix = cameraData.viewproj * modelMatrix;
    gl_Position = transformMatrix * vec4( v3Position, 1.0 );
    fragColor = vec3( v3Color );
    texCoord = v2TexCoord;
}