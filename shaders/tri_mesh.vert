#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( location = 0 ) in vec3 v3Position;
layout( location = 1 ) in vec3 v3Normal;
layout( location = 2 ) in vec3 v3Color;

layout( location = 0 ) out vec3 fragColor;

layout( push_constant ) uniform constants
{
    vec4 data;
    mat4 renderMatrix;
} PushConstants;

void main()
{
    gl_Position = PushConstants.renderMatrix * vec4( v3Position, 1.0 );
    fragColor = vec3( v3Color );
}