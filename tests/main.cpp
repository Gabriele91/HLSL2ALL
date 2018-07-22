//
//  main.cpp
//  HLSL2ALL
//
//  Created by Gabriele Di Bari on 11/07/18.
//  Copyright © 2018 University of Perugia. All rights reserved.
//

#include <iostream>
#include "HLSL2ALL/HLSL2ALL.h"


static inline void replace_all(std::string& source, std::string const& find, std::string const& replace)
{
    for (std::string::size_type i = 0; (i = source.find(find, i)) != std::string::npos;)
    {
        source.replace(i, find.length(), replace);
        i += replace.length();
    }
}

int main(int argc, const char * argv[])
{
    using namespace HLSL2ALL;
    std::string hlsl_filename("test.hlsl");
    std::string hlsl_source(
R"HLSL(
#define IVec2 int2
#define IVec3 int3
#define IVec4 int4
#define IMat3 int3x3
#define IMat4 int4x4
#define Vec2 float2
#define Vec3 float3
#define Vec4 float4
#define Mat3 float3x3
#define Mat4 float4x4
#define DVec2 double2
#define DVec3 double3
#define DVec4 double4
#define DMat3 double3x3
#define DMat4 double4x4
)HLSL");
    hlsl_source += R"HLSL(
    #line 45
    struct VertexShaderInput
    {
        Vec3 m_position : POSITION;
        Vec3 m_color    : COLOR0;
    };
    
    struct VertexShaderOutput
    {
        Vec4 m_position : SV_POSITION;  // interpolated vertex position (system value)
        Vec4 m_color    : COLOR0;       // interpolated diffuse color
    };
    
    cbuffer Camera : register(b0)
    {
        Vec4 camera_viewport;
        Mat4 camera_projection;
        Mat4 camera_view;
        Mat4 camera_model;
        Vec3 camera_position;
    };
    
    cbuffer Transform : register(b1)
    {
        Mat4 model_model;
        Vec3 model_position;
        Vec3 model_scale;
        Mat3 model_rotation;
    };
    
    //global uniform
    float mask;
    Vec4 color;

    //texture
	uniform sampler2D diffuse_texture;

    VertexShaderOutput vertex(VertexShaderInput input)
    {
        
        Vec4 position = Vec4(input.m_position, 1.0f);
        position = mul(position, model_model);
        position = mul(position, camera_view);
        position = mul(position, camera_projection);
        VertexShaderOutput output;
        output.m_position = position;
        output.m_color = Vec4(input.m_color.rgb, 1.0f);
        
        return output;
    }
    
    Vec4 fragment(VertexShaderOutput input) : SV_TARGET
    {
        Vec4 texture_color = tex2D(diffuse_texture, Vec2(1.0,1.0));
        if (texture_color.a <= mask) discard;
        return  color * Vec4(texture_color.rgb,1.0);
    }
    )HLSL";
    //inputs
    std::string shader_target_name[ST_N_SHADER]
    {
        "vertex"
        , "fragment"
        , "geometry"
        , "tass_control"
        , "tass_eval"
        , "compute"
    };
    //save types
    HLSL2ALL::TypeSpirvShaderList shader_spirv_outputs;
    HLSL2ALL::ErrorSpirvShaderList shader_spirv_errors;
    HLSL2ALL::InfoSpirvShaderList shader_spirv_info
    {
        { HLSL2ALL::ST_VERTEX_SHADER, shader_target_name[HLSL2ALL::ST_VERTEX_SHADER] },
        { HLSL2ALL::ST_TASSELLATION_CONTROL_SHADER, shader_target_name[HLSL2ALL::ST_TASSELLATION_CONTROL_SHADER] },
        { HLSL2ALL::ST_TASSELLATION_EVALUATION_SHADER, shader_target_name[HLSL2ALL::ST_TASSELLATION_EVALUATION_SHADER] },
        { HLSL2ALL::ST_GEOMETRY_SHADER, shader_target_name[HLSL2ALL::ST_GEOMETRY_SHADER] },
        { HLSL2ALL::ST_FRAGMENT_SHADER, shader_target_name[HLSL2ALL::ST_FRAGMENT_SHADER] },
        { HLSL2ALL::ST_COMPUTE_SHADER, shader_target_name[HLSL2ALL::ST_COMPUTE_SHADER] },
    };
    //info target
    TargetShaderInfo spirv_info;
    spirv_info.m_client_version = 450;
    spirv_info.m_desktop = true;
    spirv_info.m_reverse_mul = true;
    spirv_info.m_vulkan = false;
    spirv_info.m_upgrade_texture_to_samples = false;
    spirv_info.m_samplerarray_to_flat = false;
    //build
    if (!hlsl_to_spirv(  hlsl_source
                       , hlsl_filename
                       , shader_spirv_info
                       , shader_spirv_outputs
                       , shader_spirv_errors
                       , spirv_info
                       ))
    {
        for(auto&& error : shader_spirv_errors)
        {
            std::cout << error << std::endl;
        }
    }
    //...
    HLSL2ALL::GLSLConfig glsl_config;
    glsl_config.m_version = 410;
    glsl_config.m_es = false;
    glsl_config.m_vulkan_semantics = false;
    glsl_config.m_rename_position_in_position0 = true;
    glsl_config.m_fixup_clipspace = true;
    glsl_config.m_flip_vert_y = false;
    glsl_config.m_enable_420pack_extension = false;
    glsl_config.m_combined_texture_samplers = false;
    glsl_config.m_force_to_push_sample_uniform_as_texture = true;
    HLSL2ALL::TextureSamplerList tex_samples;
    //to GLSL
    //spirv to glsl
    for (const TypeSpirvShader& ssoutput : shader_spirv_outputs)
    {
        std::cout << "----------------- SHADER" << std::endl;
        //unpack
        int type = ssoutput.m_type;
        auto& shader_spirv_out = ssoutput.m_shader;
        //output shader
        std::string glsl_output;
        //first
        glsl_config.m_rename_input_with_semantic = type == HLSL2ALL::ST_VERTEX_SHADER;
        //Apple bug
        glsl_config.m_rename_input_with_locations = glsl_config.m_rename_output_with_locations;
        glsl_config.m_input_prefix = glsl_config.m_output_prefix;
        glsl_config.m_rename_output_with_locations = type != HLSL2ALL::ST_COMPUTE_SHADER;
        glsl_config.m_output_prefix = std::string("__") + shader_target_name[type];
        //compile
        if (!HLSL2ALL::spirv_to_glsl(shader_spirv_out, glsl_output, tex_samples, shader_spirv_errors, glsl_config))
        {
            for(auto& err : shader_spirv_errors)
            {
                std::cout << err << std::endl;
            }
            return false;
        }
        //cpp
        std::cout << glsl_output << std::endl;
    }
    
    return 0;
}
