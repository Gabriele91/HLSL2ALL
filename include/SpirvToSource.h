//
//  SpirvToSource.h
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#pragma once
#include <string>

namespace Square
{
    //Spirv
    using SpirvShader  = std::vector<unsigned int>;
    //config
    struct GLSLConfig
    {
        int  m_version{ 410 };
        bool m_es{ false };
        bool m_fixup_clipspace{ true };
        bool m_flip_vert_y{ true };
        bool m_rename_input_with_semantic{ false };
        std::string m_semanti_prefix{ "in_" };
    };
    //convert
    bool spirv_to_glsl
    (
       SpirvShader shader
     , std::string& source_glsl
     , const GLSLConfig& config = GLSLConfig()
    );  
	//convert
    bool spirv_to_hlsl
	(
		  SpirvShader shader
		, std::string& source_hlsl
		, int HLSL_client_version = 51
	);
}
