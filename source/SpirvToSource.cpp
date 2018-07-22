//
//  SpirvToSource.cpp
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#include <cstring>
#include <cctype>
#include <algorithm>
#include "HLSL2ALL/SpirvToSource.h"
#include "spirv.hpp"
#include "spirv_glsl.hpp"
#include "spirv_hlsl.hpp"
#include "spirv_reflect.hpp"
#include "spirv_cross_util.hpp"

namespace HLSL2ALL
{

static void replace_input_with_semantic(spirv_cross::Compiler& compiler, const std::string& prefix, bool rn_pos_in_pos0 = false)
{
    //info
    auto active = compiler.get_active_interface_variables();
    spirv_cross::ShaderResources resources = compiler.get_shader_resources(active);
    //replace
    for(const auto& r : resources.stage_inputs)
    {
        //var name: r.name;
        if (!compiler.has_decoration(r.id, spv::DecorationLocation))
            continue;
        if (!compiler.has_decoration(r.id, spv::DecorationHlslSemanticGOOGLE))
            continue;
        //get info
        int location = compiler.get_decoration(r.id, spv::DecorationLocation);
        std::string semantic = compiler.get_decoration_string(r.id, spv::DecorationHlslSemanticGOOGLE);
		//rename
		if (rn_pos_in_pos0)
		{
			//name ref
			std::string position("POSITION");
			//same size
			bool is_position = position.size() == semantic.size();
			//test
			for (size_t i = 0; i!=position.size() && is_position; ++i)
			{ 
				is_position = std::toupper(semantic[i]) == position[i];
			}
			//change
			if (is_position) semantic += '0';
		}
        //new name
        std::string new_name = prefix + semantic;
        //rename
        spirv_cross_util::rename_interface_variable(compiler, resources.stage_inputs, location, new_name);
    }
}

static void replace_with_location(spirv_cross::Compiler& compiler, const std::string& prefix, const std::vector<spirv_cross::Resource>& resources)
{
	//replace
    for(const auto& r : resources)
    {
        //var name: r.name;
        if (!compiler.has_decoration(r.id, spv::DecorationLocation))
            continue;
        //get info
        int location = compiler.get_decoration(r.id, spv::DecorationLocation);
        //new name
        std::string new_name = prefix + "__location" + std::to_string((long)location);
        //rename
        spirv_cross_util::rename_interface_variable(compiler, resources, location, new_name);
    }
}

static void add_sample_uniforms(spirv_cross::Compiler& compiler, std::string& output_source, const std::vector<spirv_cross::Resource>& resources)
{
	//replace
    for(const auto& r : resources)
    {
		//type
		auto &type = compiler.get_type(r.base_type_id);
		//type
		if (type.basetype == spirv_cross::SPIRType::Sampler)
		{
			//name
			std::string name = compiler.get_name(r.id);
			//
			switch(type.image.dim)
			{
				case spv::Dim::Dim1D:
					output_source = std::string("uniform sample1D ") + name + output_source;
				break;
				case spv::Dim::Dim2D:
					output_source = "uniform sample2D " + name + output_source;
				break;
				case spv::Dim::Dim3D:
					output_source = "uniform sample3D " + name + output_source;
				break;
				case spv::Dim::DimCube:
					output_source = "uniform sampleCube " + name + output_source;
				break;
				default:
					break;
			}			 
		}
    }
}
//convert
extern bool spirv_to_glsl
(
  SpirvShader spirv_binary
, std::string& source_glsl
, TextureSamplerList& texture_samplers
, ErrorSpirvShaderList& errors
, const GLSLConfig& config
)
{
    //init
    spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));
    spirv_cross::CompilerGLSL::Options options;
    options.version = config.m_version;
    options.es = config.m_es;
    options.vertex.fixup_clipspace = config.m_fixup_clipspace;
    options.vertex.flip_vert_y = config.m_flip_vert_y;
	options.vulkan_semantics = config.m_vulkan_semantics;
    options.enable_420pack_extension =config.m_enable_420pack_extension;
    glsl.set_common_options(options);
    //info
    if(config.m_rename_input_with_semantic)
    {
        replace_input_with_semantic(glsl, config.m_semantic_prefix, config.m_rename_position_in_position0);
    }
	//replace input
	if(config.m_rename_input_with_locations)
	{
    	auto active = glsl.get_active_interface_variables();
    	auto resources = glsl.get_shader_resources(active);
		replace_with_location(glsl, config.m_input_prefix, resources.stage_inputs);
	}
	//replace output
	if(config.m_rename_output_with_locations)
	{
    	auto active = glsl.get_active_interface_variables();
    	auto resources = glsl.get_shader_resources(active);
		replace_with_location(glsl, config.m_output_prefix, resources.stage_outputs);
	}
	//combine
	if(config.m_combined_texture_samplers)
	{
		glsl.build_dummy_sampler_for_combined_images();
		glsl.build_combined_image_samplers();
	}
    //compile
	source_glsl = glsl.compile();
	//force to add sample uniforms
	if(config.m_force_to_push_sample_uniform_as_texture && !options.vulkan_semantics)
	{
    	auto active = glsl.get_active_interface_variables();
    	auto resources = glsl.get_shader_resources(active);
		add_sample_uniforms(glsl, source_glsl, resources.separate_samplers);
	}
    //return
    return true;
}

//convert
extern bool spirv_to_hlsl
(
  SpirvShader spirv_binary
, std::string& source_hlsl
, ErrorSpirvShaderList& errors
, const HLSLConfig& config
) 
{
	//init
	spirv_cross::CompilerHLSL hlsl(std::move(spirv_binary));
	spirv_cross::CompilerHLSL::Options options;
	options.shader_model = config.m_hlsl_version;
    options.point_coord_compat = config.point_coord_compat;
    options.point_size_compat = config.point_size_compat;
	hlsl.set_hlsl_options(options);
	//compile
	try
	{
		//compile
		source_hlsl = hlsl.compile();
		return true;
	}
	catch (std::exception e)
	{
		//fail
        errors.push_back(e.what());
	}
	//return
	return false;
}

}
