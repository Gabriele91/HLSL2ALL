//
//  SourceToSpirv.cpp
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#define ENABLE_HLSL
#define NV_EXTENSIONS
#define AMD_EXTENSIONS
#include <SPIRV/doc.h>
#include <SPIRV/disassemble.h>
#include <SPIRV/SPVRemapper.h>
#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/MachineIndependent/ParseHelper.h>
#include "Square/Config.h"
#include "Square/Render/ShaderUtils/SourceToSpirv.h"
#include "Square/Core/SmartPointers.h"
#include "Square/Driver/Render.h"
#include <string>
#include <vector>

namespace glslang
{
static const TBuiltInResource DefaultTBuiltInResource =
{
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};
}

namespace Square
{
static EShLanguage render_shader_type_to_glslang_type(Square::Render::ShaderType type)
{
    switch (type)
    {
        case Square::Render::ST_VERTEX_SHADER: return EShLangVertex;
        case Square::Render::ST_FRAGMENT_SHADER: return EShLangFragment;
        case Square::Render::ST_GEOMETRY_SHADER: return EShLangGeometry;
        case Square::Render::ST_TASSELLATION_CONTROL_SHADER: return EShLangTessControl;
        case Square::Render::ST_TASSELLATION_EVALUATION_SHADER: return EShLangTessEvaluation;
        case Square::Render::ST_COMPUTE_SHADER: return EShLangCompute;
        //default
        case Square::Render::ST_N_SHADER: default: return EShLangCount;
    }
}

static std::vector<std::string> get_function_list(glslang::TIntermediate* interm)
{
	//name shape
	using namespace spv;
	using namespace glslang;
	using namespace Square;
	std::vector<std::string> output;
	//test
	if (!interm) return {};
	if (!interm->getTreeRoot()) return {};
	if (!interm->getTreeRoot()->getAsAggregate()) return {};
	//visit
	TIntermSequence& globals = interm->getTreeRoot()->getAsAggregate()->getSequence();
	for (unsigned int f = 0; f < globals.size(); ++f) 
	{
		TIntermAggregate* candidate = globals[f]->getAsAggregate();
		if (candidate && candidate->getOp() == EOpFunction && candidate->getName().c_str())
		{
			std::string name = candidate->getName().c_str();
			auto start = name.find('@'); 
			auto end = name.find('(');
			if (start != std::string::npos && end != std::string::npos)
				output.push_back(name.substr(start + 1, end - start - 1));
		}
	}
	//return
	return output;
}

class GLSlangContext
{
public:
    GLSlangContext()  { glslang::InitializeProcess(); }
    ~GLSlangContext() { glslang::FinalizeProcess(); }
};

extern bool hlsl_to_spirv
(
      const std::string&              hlsl_source
    , const std::string&              hlsl_filename
    , const std::vector<ShaderInfo>&  shaders_info
	, TypeSpirvShaderList&			  shaders_output
	, ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info
)
{
    //Glslang scope
    GLSlangContext unique_glslang_context;
    //name shape
    using namespace spv;
    using namespace glslang;
    using namespace Square;
	//select fiels
	EShTargetLanguageVersion tllanguage = ! target_info.m_vulkan ? EShTargetSpv_1_0 
										  : target_info.m_client_version == 100 ? EShTargetSpv_1_0 
										  : EShTargetSpv_1_3;
	EShTargetClientVersion tcversion = ! target_info.m_vulkan ? EShTargetOpenGL_450
									   : target_info.m_client_version == 100 ? EShTargetVulkan_1_0
									   : EShTargetVulkan_1_1;
	EShClient client = target_info.m_vulkan ? EShClientVulkan : EShClientOpenGL;
	//EOptionDefaultDesktop
	int default_version = target_info.m_desktop ? 110 : 100;
    //type of spirv
    const int clent_semantic_version = 100;   // maps to, say, #define VULKAN 100
	const int flags_messages =  EShMsgDefault
								//hlsl
								| EShMsgReadHlsl
							    | EShMsgHlslOffsets
							    | EShMsgHlslLegalization
								//spirv builds rules
								| EShMsgVulkanRules
								| EShMsgSpvRules
								//debug
								| EShMsgDebugInfo 
								//vulkan
								| (target_info.m_vulkan ? EShMsgVulkanRules : 0);
    //shaders
    TProgram program;
	std::vector< int > types;
	std::vector< Shared<TShader> > shaders;
	EShMessages messages =  EShMessages(flags_messages);
    //source
    const char* source_hlsl   = hlsl_source.c_str();
    const char* sources[]     = { source_hlsl };
    const int   length_hlsl   = (int)hlsl_source.size();
    const int   lengths[]     = { length_hlsl };
    const char* filename_hlsl = hlsl_filename.c_str();
    const char* filenames[]   = { filename_hlsl };
    //configure
	for (auto shader_info : shaders_info)
	{
		auto shader_type = render_shader_type_to_glslang_type(shader_info.m_type);
		auto shader = std::make_shared<TShader>(shader_type);
		shader->setStringsWithLengthsAndNames(sources, lengths, filenames, 1);
		shader->setEntryPoint(shader_info.m_name.c_str());		
		//by target selected
		shader->setEnvInput(EShSourceHlsl, shader_type, client, clent_semantic_version);
		shader->setEnvClient(client, tcversion);
		shader->setEnvTarget(EShTargetSpv, tllanguage);
        //mapping
        shader->setTextureSamplerTransformMode(EShTexSampTransKeep);
        shader->setHlslIoMapping(true); //16bit
        shader->setAutoMapBindings(true);
        shader->setAutoMapLocations(true);
		shader->setEnvTargetHlslFunctionality1();
        //compiler info
        TBuiltInResource resources = DefaultTBuiltInResource;
        //parse
        if(!shader->parse(  &resources
                          , default_version
                          , ENoProfile
                          , false
                          , false
                          , messages ))
        {
            errors.push_back("Shader compile error");
            errors.push_back(shader_info.m_name);
			errors.push_back({ shader->getInfoLog() });
            return false;
        }
		//test
		for (std::string& function : get_function_list(shader->getIntermediate()))
		{
			if (function == shader_info.m_name)
			{
				//save type
				types.push_back(shader_info.m_type);
				//save shader
				shaders.push_back(shader);
				//add to program
				program.addShader(shader.get());
			}
		}
    }
    //liking
    if (!program.link(messages))
    {
        errors.push_back("Program linking error");
		errors.push_back({ program.getInfoLog() });
        return false;
    }
	//build info
	program.buildReflection();
	//alloc
	shaders_output.resize(shaders.size());
    //get spriv
    for(size_t i = 0; i!= shaders_output.size(); ++i)
    {
        //get type
        auto shader_type = render_shader_type_to_glslang_type((Render::ShaderType)types[i]);
        //get spv
        {
            //errors
            SpvBuildLogger logger;
            SpvOptions spv_options;
			spv_options.generateDebugInfo = true;
			spv_options.disableOptimizer = false;
			spv_options.optimizeSize = true;
			//shader output
			SpirvShader output;
			//get output
			GlslangToSpv(*program.getIntermediate(shader_type), output, &logger, &spv_options);
			//spv::SpirvToolsDisassemble(std::cout, output);
			spv::Disassemble(std::cout, output);
			//build
			std::get<0>(shaders_output[i]) = types[i];
			std::get<1>(shaders_output[i]) = std::move(output);
        }
    }
    return shaders_output.size();
}
}
