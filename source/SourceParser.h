//
//  SourceParser.h
//  HLSL2ALL
//
//  Created by Gabriele Di Bari.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
//  Scans of the HLSL source before glslang (internal to HLSL2ALL).
//
#pragma once
#include <string>

namespace HLSL2ALL
{
namespace Parser
{
	//the source can have a function `name`: the entry points not in the source are skipped
	//without parsing it (a parse for each of the stages costs, almost all of the time of a
	//shader). The name is a function when it is followed by '(' in the code (comments and
	//directives skipped), or when it is in a #define (an entry point made by a macro, e.g.
	//`#define surface SurfaceOutput fragment`). A false positive only costs that parse.
	bool source_may_have_function(const std::string& source, const std::string& name);
}
}
