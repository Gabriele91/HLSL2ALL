//
//  SpirvToSource.cpp
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#include <iostream>
#include "SpirvToSource.h"
#include <spirv_cross/spirv_hlsl.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_reflect.hpp>
#include <spirv_cross/spirv_cross_util.hpp>
namespace Square
{

static void replace_input_with_semantic(spirv_cross::Compiler& compiler, const std::string& prefix)
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
        //new name
        std::string new_name = prefix + semantic;
        //rename
        spirv_cross_util::rename_interface_variable(compiler, resources.stage_inputs, location, new_name);
    }
}
    
//convert
extern bool spirv_to_glsl
(
  SpirvShader spirv_binary
, std::string& source_glsl
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
	options.vulkan_semantics = false;
    glsl.set_common_options(options);
    //info
    if(config.m_rename_input_with_semantic)
    {
        replace_input_with_semantic(glsl,config.m_semanti_prefix);
    }
    //compile
	source_glsl = glsl.compile();
    //return
    return true;
}

//convert
extern bool spirv_to_hlsl
(
	 SpirvShader spirv_binary
	, std::string& source_hlsl
	, int HLSL_client_version
) 
{
	//init
	spirv_cross::CompilerHLSL hlsl(std::move(spirv_binary));
	spirv_cross::CompilerHLSL::Options options;
	options.shader_model = HLSL_client_version;
	hlsl.set_hlsl_options(options);
	//attrs
	std::vector< spirv_cross::HLSLVertexAttributeRemap > attrs
	{
		{ 0, "POSTION0" },
	    { 1, "COLOR0" },
	};
	//compile
	try
	{
		//
		source_hlsl = hlsl.compile();
		return true;
	}
	catch (std::exception e)
	{
		//none
	}
	//return
	return false;
}

}
