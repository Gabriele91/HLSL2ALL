//
//  SpirvToSource.h
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#pragma once
#include "HLSL2ALL/HLSL2ALLConfig.h"

namespace HLSL2ALL
{
    //Spirv
    using SpirvShader  = std::vector<unsigned int>;
	//errors
	using ErrorSpirvShaderList = std::vector<std::string>;
    //config
    struct GLSLConfig
    {
        int  m_version{ 410 };
        bool m_es{ false };
        bool m_fixup_clipspace{ true };
        bool m_flip_vert_y{ true };
        bool m_rename_input_with_semantic{ false };
		bool m_rename_position_in_position0{ false };
        std::string m_semanti_prefix{ "in_" };
    };
    //convert
    HLSL2ALL_API bool spirv_to_glsl
    (
        SpirvShader shader
    , std::string& source_glsl
    , ErrorSpirvShaderList& errors
    , const GLSLConfig& config = GLSLConfig()
    );
    //HLSLConfig
    struct HLSLConfig
    {
        int m_hlsl_version{ 50 };
        bool point_size_compat{ false };
        bool point_coord_compat { false };
    };
	//convert
    HLSL2ALL_API bool spirv_to_hlsl
    (
        SpirvShader shader
    , std::string& source_hlsl
    , ErrorSpirvShaderList& errors
    , const HLSLConfig& config = HLSLConfig()
    );
}
