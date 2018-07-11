//
//  SourceToSpirv.h
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#pragma once
#include "Square/Config.h"
#include "Square/Driver/Render.h"

namespace Square
{
    //Shader Info
    struct ShaderInfo
    {
        Square::Render::ShaderType m_type;
        std::string                m_name;
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
	};
    //convert
	SQUARE_API bool hlsl_to_spirv
    (
      const std::string&              hlsl_source
    , const std::string&              hlsl_filename
    , const std::vector<ShaderInfo>&  shaders_info
    , TypeSpirvShaderList&			  shaders_output
    , ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info = TargetShaderInfo()
    );
}
