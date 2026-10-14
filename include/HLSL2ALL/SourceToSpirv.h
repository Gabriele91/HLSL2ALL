//
//  SourceToSpirv.h
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#pragma once
#include "HLSL2ALL/HLSL2ALLConfig.h"

namespace HLSL2ALL
{
    //the process of glslang for the whole application, before any conversion and after the
    //last one (counted: a shutdown for each initialize); its built-in symbol tables are made
    //once, for all the conversions
    HLSL2ALL_API bool initialize();
    HLSL2ALL_API void shutdown();

    //type of shader
    enum ShaderType
    {
        ST_VERTEX_SHADER,
        ST_FRAGMENT_SHADER,
        ST_GEOMETRY_SHADER,
        ST_TASSELLATION_CONTROL_SHADER,
        ST_TASSELLATION_EVALUATION_SHADER,
        ST_COMPUTE_SHADER,
        ST_N_SHADER
    };
    //Shader Info
    struct ShaderInfo
    {
        ShaderType  m_type;
        String m_name;
    };
    //Spirv
    using SpirvShader  = Vector<unsigned int>;
	//Spirv
    struct TypeSpirvShader
    {
        int m_type;
        SpirvShader m_shader;
    };
	using TypeSpirvShaderList = Vector< TypeSpirvShader >;
    //Hlsl
    using HLSLShader  = String;
	//Spirv
    struct TypeHLSLShader
    {
        int m_type;
        HLSLShader m_shader;
    };
	using TypeHLSLShaderList = Vector< TypeHLSLShader >;
	//errors
	using ErrorSpirvShaderList = Vector<String>;
	//errors
	using InfoSpirvShaderList = Vector< ShaderInfo >;
	//Target Shader Info
	struct TargetShaderInfo
	{
		int   m_client_version{450};
		bool  m_vulkan{ false };
		bool  m_desktop{ true };
        bool  m_reverse_mul{ false };
        bool  m_samplerarray_to_flat{ true };
        bool  m_upgrade_texture_to_samples{ true };
        //the layout of a driver of Vulkan (with m_vulkan): the resources of all the stages of a
        //program in one descriptor set (set 0), one binding for each of them, the same in every
        //stage (by its name: the constant buffers by the name of their block, first, then the
        //textures and the samplers by the name of their variable), not the registers of glslang;
        //without the HLSL decorations of glslang (SPV_GOOGLE_hlsl_functionality1, user_type): plain
        //SPIR-V of Vulkan
        bool  m_vulkan_layout{ false };
        //with m_vulkan_layout: the locations of the inputs of the vertex stage by their semantic
        //("POSITION0", "TEXCOORD1": its index at its end, 0 when it has none); the inputs of a
        //semantic not here keep their location
        Vector< std::pair<String, unsigned int> > m_vertex_input_locations;
	};
    
    //convert
    HLSL2ALL_API bool hlsl_to_spirv
    (
      const String&              hlsl_source
    , const String&              hlsl_filename
    , const Vector<ShaderInfo>&  shaders_info
    , TypeSpirvShaderList&			  shaders_output
    , ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info = TargetShaderInfo()
    );
    
    //convert
    HLSL2ALL_API bool hlsl_to_hlsl_preprocessed
    (
      const String&              hlsl_source
    , const String&              hlsl_filename
    , const Vector<ShaderInfo>&  shaders_info
    , TypeHLSLShaderList&			  shaders_output
    , ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info = TargetShaderInfo()
    , bool                            remove_shader_wo_entrypoint = false
    );
}
