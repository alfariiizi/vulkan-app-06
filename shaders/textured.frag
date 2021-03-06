#version 450
#extension GL_ARB_separate_shader_objects : enable

// input write
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 outFragColor;

layout( set = 0, binding = 1 ) uniform GpuSceneParameterData
{
    vec4 fogColor;     // w is for exponent
    vec4 fogDistance;  // x for min, y for max, z and w are unused
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
}sceneParameterData;

layout( set = 2, binding = 0 ) uniform sampler2D tex1;

void main()
{
    vec3 color = texture( tex1, texCoord ).xyz;
    outFragColor = vec4( color, 1.0f );
}
