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
	//texture + sampler
	struct TextureSampler
	{
		std::string m_combination;
		std::string m_texture;
		std::string m_sampler;
		int m_combination_id;
		int m_texture_id;
		int m_sampler_id;
	};
	using TextureSamplerList = std::vector< TextureSampler >;
    //config
    struct GLSLConfig
    {
        int  m_version{ 410 };
        bool m_es{ false };
        bool m_vulkan_semantics{ false };
        bool m_fixup_clipspace{ true };
        bool m_flip_vert_y{ true };
        bool m_enable_420pack_extension{ false };
        //combine shaders
        bool m_combined_texture_samplers{ true };
        //semantic
        bool m_rename_input_with_semantic{ false };
        std::string m_semantic_prefix{ "in_" };
        //rename POSITION in POSITION0
        bool m_rename_position_in_position0{ false };
        //rename input with prefix__location[N]
        bool m_rename_input_with_locations{ false };
        std::string m_input_prefix{ "" };
        //rename output with prefix__location[N]
        bool m_rename_output_with_locations{ false };
        std::string m_output_prefix{ "" };
    };
    //convert
    HLSL2ALL_API bool spirv_to_glsl
    (
      SpirvShader shader
    , std::string& source_glsl
	, TextureSamplerList& texture_samplers
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
