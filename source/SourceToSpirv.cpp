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
#include "HLSL2ALL/SourceToSpirv.h"
#include <string>
#include <vector>
#include <memory>
#include <SPIRV/doc.h>
#include <SPIRV/disassemble.h>
#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/MachineIndependent/ParseHelper.h>
#include <glslang/Public/ResourceLimits.h>

namespace HLSL2ALL
{
// glslang now provides the default resource limits; use its builtin instead
// of the (now layout-incompatible) hand-rolled table.
#define DefaultTBuiltInResource (*GetDefaultResources())
}

namespace HLSL2ALL
{
static EShLanguage render_shader_type_to_glslang_type(ShaderType type)
{
    switch (type)
    {
        case ST_VERTEX_SHADER: return EShLangVertex;
        case ST_FRAGMENT_SHADER: return EShLangFragment;
        case ST_GEOMETRY_SHADER: return EShLangGeometry;
        case ST_TASSELLATION_CONTROL_SHADER: return EShLangTessControl;
        case ST_TASSELLATION_EVALUATION_SHADER: return EShLangTessEvaluation;
        case ST_COMPUTE_SHADER: return EShLangCompute;
        //default
        case ST_N_SHADER: default: return EShLangCount;
    }
}

static ShaderType glslang_type_to_render_shader_type(EShLanguage type)
{
    switch (type)
    {
    case EShLangVertex: return ST_VERTEX_SHADER;
    case EShLangFragment: return ST_FRAGMENT_SHADER;
    case EShLangGeometry: return ST_GEOMETRY_SHADER;
    case EShLangTessControl: return ST_TASSELLATION_CONTROL_SHADER;
    case EShLangTessEvaluation: return ST_TASSELLATION_EVALUATION_SHADER;
    case EShLangCompute: return ST_COMPUTE_SHADER;
        //default
    case EShLangCount: default: return ST_N_SHADER;
    }
}

static std::vector<std::string> get_function_list(glslang::TIntermediate* interm)
{
	//name shape
	using namespace spv;
	using namespace glslang;
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
	//select fiels
	EShTargetLanguageVersion tllanguage =  (!target_info.m_vulkan) 
                                          ? EShTargetSpv_1_0 
										  : target_info.m_client_version == 100 
                                          ? EShTargetSpv_1_0 
										  : EShTargetSpv_1_3;
	EShTargetClientVersion tcversion = ! target_info.m_vulkan ? EShTargetOpenGL_450
									   : target_info.m_client_version == 100 ? EShTargetVulkan_1_0
									   : EShTargetVulkan_1_1;
	EShClient client = target_info.m_vulkan ? EShClientVulkan : EShClientOpenGL;
	//EOptionDefaultDesktop
	int default_version = target_info.m_desktop ? 110 : 100;
    //chouse mode to skeep texture/sampler
    EShTextureSamplerTransformMode tmode = target_info.m_upgrade_texture_to_samples 
                                        ? EShTexSampTransUpgradeTextureRemoveSampler 
                                        : EShTexSampTransKeep;
    //flat array of texture
    bool ta_to_flat = target_info.m_samplerarray_to_flat;
    //type of spirv
    const int clent_semantic_version = 100;   // maps to, say, #define VULKAN 100
	const int flags_messages =  EShMsgDefault
								//hlsl
								| EShMsgReadHlsl
							    | EShMsgHlslOffsets
							  //| EShMsgHlslLegalization
                              //| EShMsgBuiltinSymbolTable
								//debug
								| EShMsgDebugInfo 
								//vulkan spirv
								| (target_info.m_vulkan ? (EShMsgVulkanRules | EShMsgSpvRules) : 0);
    //shaders
    TProgram program;
	std::vector< int > types;
    std::vector< std::shared_ptr<TShader> > shaders;
	EShMessages messages =  EShMessages(flags_messages);
    //source_ptrs
    std::vector<const char*> sources;
    //source_ptrs
    std::vector<const char*> filenames;
    //source lens
    std::vector< int > lengths;
    //ptr of fake file
    const char* inv_mul_str = "#define mul(x,y) mul(y,x)\n";
    const char* inv_mul_name = "inv_mul.hlsl";
    //source
    if(target_info.m_reverse_mul)
    {
        sources.push_back(inv_mul_str);
        filenames.push_back(inv_mul_name);
        lengths.push_back((int)std::strlen(inv_mul_str));
    }
    sources.push_back(hlsl_source.c_str());
    filenames.push_back( hlsl_filename.c_str());
    lengths.push_back((int)hlsl_source.size());
    //configure
	for (auto shader_info : shaders_info)
	{
		auto shader_type = render_shader_type_to_glslang_type(shader_info.m_type);
		auto shader = std::make_shared<TShader>(shader_type);
		shader->setStringsWithLengthsAndNames(sources.data(), lengths.data(), filenames.data(), (int)sources.size());
		shader->setEntryPoint(shader_info.m_name.c_str());		
		//by target selected
		shader->setEnvInput(EShSourceHlsl, shader_type, client, clent_semantic_version);
		shader->setEnvClient(client, tcversion);
		shader->setEnvTarget(EShTargetSpv, tllanguage);
        //texture
        shader->setTextureSamplerTransformMode(tmode);
        shader->setFlattenUniformArrays(ta_to_flat);
        //mapping
        shader->setNoStorageFormat(true);
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
    // I/O mapping: actually runs the auto-map binding/location resolver enabled
    // via setAutoMapBindings/setAutoMapLocations. Without this, every cbuffer
    // keeps the default binding 0, so they all collide at set 0 / binding 0;
    // SPIRV-Cross then aliases them onto a single MSL buffer (spvBufferAliasSet0Binding0)
    // and the reflection loses all but one cbuffer name. mapIO() gives each
    // resource a distinct binding so SPIRV-Cross emits separate buffers.
    if (!program.mapIO())
    {
        errors.push_back("Program I/O mapping error");
        errors.push_back({ program.getInfoLog() });
        return false;
    }
	//alloc
	shaders_output.resize(shaders.size());
    //get spriv
    for(size_t i = 0; i!= shaders_output.size(); ++i)
    {
        //get type
        auto shader_type = render_shader_type_to_glslang_type((ShaderType)types[i]);
        //get spv
        {
            //errors
            SpvBuildLogger logger;
            SpvOptions spv_options;
			spv_options.generateDebugInfo = true;
			spv_options.disableOptimizer = false;
			spv_options.optimizeSize = true;
            // spv_options.validate = true;
			//shader output
			SpirvShader output;
			//get output
			GlslangToSpv(*program.getIntermediate(shader_type), output, &logger, &spv_options);
			//build
            shaders_output[i].m_type = types[i];
            shaders_output[i].m_shader = output;
        }
    }
    return shaders_output.size();
}

static bool entry_point_is_present
(
      glslang::TShader& shader
    , const TBuiltInResource& resources
    , int version
    , EShMessages messages
    , ShaderInfo& shader_info
)
{
    // Not full compilation
    messages = static_cast<EShMessages>(messages | EShOptNoGeneration);
    // Parse
    if (!shader.parse(&resources, version, ENoProfile, false, false, messages))
    {
        return false;
    }
    else
    {
        for (std::string& function : get_function_list(shader.getIntermediate()))
        {
            if (function == shader_info.m_name)
            {
                return true;
            }
        }
    }
    return false;
}

extern bool hlsl_to_hlsl_preprocessed
(
      const std::string&              hlsl_source
    , const std::string&              hlsl_filename
    , const std::vector<ShaderInfo>&  shaders_info
	, TypeHLSLShaderList&			  shaders_output
	, ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info
    , bool                            remove_shader_wo_entrypoint
)
{
    //Glslang scope
    GLSlangContext unique_glslang_context;
    //name shape
    using namespace spv;
    using namespace glslang;
	//select fiels
	EShTargetLanguageVersion tllanguage =  (!target_info.m_vulkan) 
                                          ? EShTargetSpv_1_0 
										  : target_info.m_client_version == 100 
                                          ? EShTargetSpv_1_0 
										  : EShTargetSpv_1_3;
	EShTargetClientVersion tcversion = ! target_info.m_vulkan ? EShTargetOpenGL_450
									   : target_info.m_client_version == 100 ? EShTargetVulkan_1_0
									   : EShTargetVulkan_1_1;
	EShClient client = target_info.m_vulkan ? EShClientVulkan : EShClientOpenGL;
	//EOptionDefaultDesktop
	int default_version = target_info.m_desktop ? 110 : 100;
    //chouse mode to skeep texture/sampler
    EShTextureSamplerTransformMode tmode = target_info.m_upgrade_texture_to_samples 
                                        ? EShTexSampTransUpgradeTextureRemoveSampler 
                                        : EShTexSampTransKeep;
    //flat array of texture
    bool ta_to_flat = target_info.m_samplerarray_to_flat;
    //type of spirv
    const int clent_semantic_version = 100;   // maps to, say, #define VULKAN 100
	int flags_messages =    EShMsgDefault
                           | EShMsgOnlyPreprocessor
							//hlsl
							| EShMsgReadHlsl
							| EShMsgHlslOffsets
							//debug
							| EShMsgDebugInfo 
							//vulkan spirv
							| (target_info.m_vulkan ? (EShMsgVulkanRules | EShMsgSpvRules) : 0);
    //shaders
    TProgram program;
	std::vector< int > types;
    std::vector< std::shared_ptr<TShader> > shaders;
	EShMessages messages =  EShMessages(flags_messages);
    //source_ptrs
    std::vector<const char*> sources;
    //source_ptrs
    std::vector<const char*> filenames;
    //source lens
    std::vector< int > lengths;
    //ptr of fake file
    const char* inv_mul_str = "#define mul(x,y) mul(y,x)\n";
    const char* inv_mul_name = "inv_mul.hlsl";
    //source
    if(target_info.m_reverse_mul)
    {
        sources.push_back(inv_mul_str);
        filenames.push_back(inv_mul_name);
        lengths.push_back((int)std::strlen(inv_mul_str));
    }
    sources.push_back(hlsl_source.c_str());
    filenames.push_back( hlsl_filename.c_str());
    lengths.push_back((int)hlsl_source.size());
    //Shared ptr
    std::unique_ptr< TShader > shader(nullptr);
    //configure
	for (auto shader_info : shaders_info)
	{
		auto shader_type = render_shader_type_to_glslang_type(shader_info.m_type);
        shader = std::make_unique<TShader>(shader_type);
		shader->setStringsWithLengthsAndNames(sources.data(), lengths.data(), filenames.data(), (int)sources.size());
		shader->setEntryPoint(shader_info.m_name.c_str());		
		//by target selected
		shader->setEnvInput(EShSourceHlsl, shader_type, client, clent_semantic_version);
		shader->setEnvClient(client, tcversion);
		shader->setEnvTarget(EShTargetSpv, tllanguage);
        //texture
        shader->setTextureSamplerTransformMode(tmode);
        shader->setFlattenUniformArrays(ta_to_flat);
        //mapping
        shader->setNoStorageFormat(true);
        shader->setHlslIoMapping(true); //16bit
        shader->setAutoMapBindings(true);
        shader->setAutoMapLocations(true);
		shader->setEnvTargetHlslFunctionality1();
        //compiler info
        TBuiltInResource resources = DefaultTBuiltInResource;
        TShader::ForbidIncluder includer;
        // Output
        HLSLShader output_shader;
        //parse
        if (!shader->preprocess(&resources
            , default_version
            , ENoProfile
            , false
            , false
            , messages
            , &output_shader
            , includer))
        {
            errors.push_back("Shader compile error");
            errors.push_back(shader_info.m_name);
            errors.push_back({ shader->getInfoLog() });
            return false;
        }
        else
        {
            if (remove_shader_wo_entrypoint)
            {
                if (entry_point_is_present(*shader, resources, default_version, messages, shader_info))
                {
                    // Add shader
                    shaders_output.emplace_back(TypeHLSLShader{ 
                        glslang_type_to_render_shader_type(shader_type),
                        std::move(output_shader) 
                    });
                }
            }
            else
            {
                // Add shader
                shaders_output.emplace_back(TypeHLSLShader{ 
                    glslang_type_to_render_shader_type(shader_type),
                    std::move(output_shader) 
                });
            }
        }
    }
    // Error?
    if (shaders_output.empty() && shader && strlen(shader->getInfoLog()) != 0)
    {
        errors.push_back("Shader compile error");
        errors.push_back({ shader->getInfoLog() });
    }
    return shaders_output.size();
}
}
