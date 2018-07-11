//
//  SpirvToSource.h
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
    //Spirv
    using SpirvShader  = std::vector<unsigned int>;
    //convert
	SQUARE_API bool spirv_to_glsl
    (
       SpirvShader shader
     , std::string& source_glsl
     , int OpenGL_client_version = 410
     , bool OpenGL_ES = false
    );  
	//convert
	SQUARE_API bool spirv_to_hlsl
	(
		  SpirvShader shader
		, std::string& source_hlsl
		, int HLSL_client_version = 51
	);
}
