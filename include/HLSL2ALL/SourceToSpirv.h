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
        std::string m_name;
    };
    //Spirv
    using SpirvShader  = std::vector<unsigned int>;
	//Spirv
	using TypeSpirvShader = std::tuple<int, SpirvShader>;
	//output
	using TypeSpirvShaderList = std::vector< std::tuple<int, SpirvShader> >;
	//errors
	using ErrorSpirvShaderList = std::vector<std::string>;
	//errors
	using InfoSpirvShaderList = std::vector< ShaderInfo >;
	//Target Shader Info
	struct TargetShaderInfo
	{
		int   m_client_version{450};
		bool  m_vulkan{ false };
		bool  m_desktop{ true };
        bool  m_reverse_mul{ false };
        bool  m_samplerarray_to_flat{ true };
        bool  m_upgrade_texture_to_samples{ true };
	};
    //convert
    HLSL2ALL_API bool hlsl_to_spirv
    (
      const std::string&              hlsl_source
    , const std::string&              hlsl_filename
    , const std::vector<ShaderInfo>&  shaders_info
    , TypeSpirvShaderList&			  shaders_output
    , ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info = TargetShaderInfo()
    );
}
