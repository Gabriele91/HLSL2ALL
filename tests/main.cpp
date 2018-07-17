//
//  main.cpp
//  HLSL2ALL
//
//  Created by Gabriele Di Bari on 11/07/18.
//  Copyright © 2018 University of Perugia. All rights reserved.
//

#include <iostream>
#include "HLSL2ALL/HLSL2ALL.h"


int main(int argc, const char * argv[])
{
    using namespace HLSL2ALL;
    std::string hlsl_filename("test.hlsl");
    std::string hlsl_source(
    "#define IVec2 int2\n"
    "#define IVec3 int3\n"
    "#define IVec4 int4\n"
    "#define IMat3 int3x3\n"
    "#define IMat4 int4x4\n"
    "#define Vec2 float2\n"
    "#define Vec3 float3\n"
    "#define Vec4 float4\n"
    "#define Mat3 float3x3\n"
    "#define Mat4 float4x4\n"
    "#define DVec2 double2\n"
    "#define DVec3 double3\n"
    "#define DVec4 double4\n"
    "#define DMat3 double3x3\n"
    "#define DMat4 double4x4\n"
    );
    hlsl_source += R"HLSL(
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
        if (input.m_color.a <= mask) discard;
        return input.m_color * color;
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
    TypeSpirvShaderList shader_spirv_outputs;
    ErrorSpirvShaderList shader_spirv_errors;
    InfoSpirvShaderList shader_spirv_info
    {
        { ST_VERTEX_SHADER, shader_target_name[ST_VERTEX_SHADER] },
        { ST_FRAGMENT_SHADER, shader_target_name[ST_FRAGMENT_SHADER] },
        { ST_GEOMETRY_SHADER, shader_target_name[ST_GEOMETRY_SHADER] },
        { ST_TASSELLATION_CONTROL_SHADER, shader_target_name[ST_TASSELLATION_CONTROL_SHADER] },
        { ST_TASSELLATION_EVALUATION_SHADER, shader_target_name[ST_TASSELLATION_EVALUATION_SHADER] },
        { ST_COMPUTE_SHADER, shader_target_name[ST_COMPUTE_SHADER] },
    };
    //build
    if (!hlsl_to_spirv(  hlsl_source
                       , hlsl_filename
                       , shader_spirv_info
                       , shader_spirv_outputs
                       , shader_spirv_errors
                       , TargetShaderInfo{ 450, false, true, true }
                       ))
    {
        for(auto&& error : shader_spirv_errors)
        {
            std::cout << error << std::endl;
        }
    }
    
    //to GLSL
    //spirv to glsl
    for (const TypeSpirvShader& ssoutput : shader_spirv_outputs)
    {
        //unpack
        int type = std::get<0>(ssoutput);
        SpirvShader shader_spirv_out = std::get<1>(ssoutput);
        //output shader
        std::string glsl_output;
        //convert
        GLSLConfig glsl_config;
        if(type == ST_VERTEX_SHADER) glsl_config.m_rename_input_with_semantic = true;
        spirv_to_glsl(shader_spirv_out, glsl_output, glsl_config);
        //cpp
        std::cout << "----------------- SHADER" << std::endl;
        std::cout << glsl_output << std::endl;
    }
    
    return 0;
}
