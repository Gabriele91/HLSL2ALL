//
//  SpirvToSource.cpp
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#include <iostream>
#include "Square/Config.h"
#include "Square/Render/ShaderUtils/SourceToSpirv.h"
#include <spirv_cross/spirv_hlsl.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_reflect.hpp>
#include <spirv_cross/spirv_cross_util.hpp>
namespace Square
{
    
//convert
extern bool spirv_to_glsl
(
  SpirvShader spirv_binary
, std::string& source_glsl
, int OpenGL_client_version
, bool OpenGL_ES
)
{
    //init
    spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));
    spirv_cross::CompilerGLSL::Options options;
    options.version = OpenGL_client_version;
	options.es = OpenGL_ES;
	options.vulkan_semantics = false;
    glsl.set_common_options(options);
	/*rename*/
	auto resources = glsl.get_shader_resources();
	//inputs
	//for (auto i : resources.stage_inputs)
	//{
	//	glsl.set_name(i.id, )
	//}
	//auto res = glsl.get_shader_resources();
	//res.stage_inputs[0].type_id
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
