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
#include "SourceParser.h"
#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <utility>
#include <unordered_map>
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

//the layout of a driver of Vulkan (TargetShaderInfo::m_vulkan_layout): the SPIR-V of the stages
//of a program read and written again, word by word
namespace VulkanLayout
{
    //the SPIR-V words used here (the specification: the opcodes, the decorations)
    static constexpr unsigned int s_header = 5;
    static constexpr unsigned int s_op_name = 5;
    static constexpr unsigned int s_op_extension = 10;
    static constexpr unsigned int s_op_type_image = 25;
    static constexpr unsigned int s_op_type_sampler = 26;
    static constexpr unsigned int s_op_type_sampled_image = 27;
    static constexpr unsigned int s_op_type_array = 28;
    static constexpr unsigned int s_op_type_runtime_array = 29;
    static constexpr unsigned int s_op_type_struct = 30;
    static constexpr unsigned int s_op_type_pointer = 32;
    static constexpr unsigned int s_op_variable = 59;
    static constexpr unsigned int s_op_decorate = 71;
    static constexpr unsigned int s_op_decorate_id = 332;
    static constexpr unsigned int s_op_decorate_string = 5632;
    static constexpr unsigned int s_op_member_decorate_string = 5633;
    static constexpr unsigned int s_decoration_location = 30;
    static constexpr unsigned int s_decoration_binding = 33;
    static constexpr unsigned int s_decoration_descriptor_set = 34;
    static constexpr unsigned int s_decoration_hlsl_counter_buffer = 5634;
    static constexpr unsigned int s_decoration_hlsl_semantic = 5635;
    static constexpr unsigned int s_storage_uniform_constant = 0;
    static constexpr unsigned int s_storage_input = 1;
    static constexpr unsigned int s_storage_uniform = 2;
    static constexpr unsigned int s_storage_storage_buffer = 12;

    //an instruction: its opcode, its words (the opcode word the first)
    struct Instruction
    {
        unsigned int m_opcode{ 0 };
        size_t       m_at{ 0 };
        size_t       m_count{ 0 };
    };

    //a resource of a stage: its variable, its name (of its block: a constant buffer), a block or not
    struct Resource
    {
        unsigned int m_variable{ 0 };
        std::string  m_name;
        bool         m_block{ false };
    };

    //what a stage has: its resources, its inputs (by their semantic), where its decorations are
    struct Stage
    {
        std::vector<Instruction>                     m_instructions;
        std::vector<Resource>                        m_resources;
        std::unordered_map<unsigned int, std::string> m_semantics;      //variable -> semantic
        std::vector<unsigned int>                     m_inputs;         //the input variables
        std::unordered_map<unsigned int, size_t>      m_binding;        //variable -> word of its binding
        std::unordered_map<unsigned int, size_t>      m_set;            //variable -> word of its set
        std::unordered_map<unsigned int, size_t>      m_location;       //variable -> word of its location
    };

    //a literal string of an instruction from a word (its bytes, up to the first zero)
    static std::string literal(const SpirvShader& words, size_t at, size_t end)
    {
        std::string out;
        for (size_t i = at; i < end; ++i)
        {
            for (unsigned int shift = 0; shift != 32; shift += 8)
            {
                const char c = char((words[i] >> shift) & 0xFF);
                if (c == 0)
                {
                    return out;
                }
                out += c;
            }
        }
        return out;
    }

    //the instructions of a module (empty: not a module)
    static std::vector<Instruction> instructions(const SpirvShader& words)
    {
        std::vector<Instruction> out;
        size_t at = s_header;
        while (at < words.size())
        {
            Instruction instruction;
            instruction.m_opcode = words[at] & 0xFFFF;
            instruction.m_at = at;
            instruction.m_count = words[at] >> 16;
            if (instruction.m_count == 0 || at + instruction.m_count > words.size())
            {
                return {};
            }
            out.push_back(instruction);
            at += instruction.m_count;
        }
        return out;
    }

    //a stage read: its resources (in the order of their variables), its inputs, its decorations
    static Stage read(const SpirvShader& words)
    {
        Stage stage;
        stage.m_instructions = instructions(words);
        std::unordered_map<unsigned int, std::string> names;
        std::unordered_map<unsigned int, unsigned int> pointee;       //pointer -> its type
        std::unordered_map<unsigned int, unsigned int> element;       //array -> its element
        std::unordered_map<unsigned int, unsigned int> kind;          //type -> its opcode
        std::vector<std::pair<unsigned int, unsigned int>> variables; //id, storage class
        std::unordered_map<unsigned int, unsigned int> variable_type;
        for (const Instruction& instruction : stage.m_instructions)
        {
            const size_t at = instruction.m_at;
            const size_t end = at + instruction.m_count;
            switch (instruction.m_opcode)
            {
            case s_op_name:
                names[words[at + 1]] = literal(words, at + 2, end);
                break;
            case s_op_type_image:
            case s_op_type_sampler:
            case s_op_type_sampled_image:
            case s_op_type_struct:
                kind[words[at + 1]] = instruction.m_opcode;
                break;
            case s_op_type_array:
            case s_op_type_runtime_array:
                element[words[at + 1]] = words[at + 2];
                break;
            case s_op_type_pointer:
                pointee[words[at + 1]] = words[at + 3];
                break;
            case s_op_variable:
                variables.push_back({ words[at + 2], words[at + 3] });
                variable_type[words[at + 2]] = words[at + 1];
                break;
            case s_op_decorate:
                if (instruction.m_count >= 4)
                {
                    const unsigned int target = words[at + 1];
                    switch (words[at + 2])
                    {
                    case s_decoration_binding:        stage.m_binding[target] = at + 3;  break;
                    case s_decoration_descriptor_set: stage.m_set[target] = at + 3;      break;
                    case s_decoration_location:       stage.m_location[target] = at + 3; break;
                    default: break;
                    }
                }
                break;
            case s_op_decorate_string:
                if (instruction.m_count >= 4 && words[at + 2] == s_decoration_hlsl_semantic)
                {
                    stage.m_semantics[words[at + 1]] = literal(words, at + 3, end);
                }
                break;
            default:
                break;
            }
        }
        //the resources: the blocks (by the name of their type), the images and the samplers
        for (const auto& variable : variables)
        {
            const unsigned int storage = variable.second;
            if (storage == s_storage_input)
            {
                stage.m_inputs.push_back(variable.first);
            }
            const bool resource = storage == s_storage_uniform_constant
                               || storage == s_storage_uniform
                               || storage == s_storage_storage_buffer;
            if (resource)
            {
                unsigned int type = pointee[variable_type[variable.first]];
                while (element.count(type))
                {
                    type = element[type];
                }
                Resource out;
                out.m_variable = variable.first;
                out.m_block = kind[type] == s_op_type_struct;
                out.m_name = out.m_block ? names[type] : names[variable.first];
                if (out.m_name.empty())
                {
                    out.m_name = names[variable.first];
                }
                const bool bindable = out.m_block
                                   || kind[type] == s_op_type_image
                                   || kind[type] == s_op_type_sampler
                                   || kind[type] == s_op_type_sampled_image;
                if (bindable)
                {
                    stage.m_resources.push_back(out);
                }
            }
        }
        return stage;
    }

    //a semantic as a name and an index at its end ("TEXCOORD1"; "POSITION": "POSITION0"), upper case
    static std::string semantic_with_index(const std::string& semantic)
    {
        std::string out;
        for (char c : semantic)
        {
            out += char(std::toupper((unsigned char)c));
        }
        if (out.empty() || !std::isdigit((unsigned char)out.back()))
        {
            out += '0';
        }
        return out;
    }

    //an instruction not written (the HLSL decorations of glslang: not of Vulkan)
    static bool removed(const SpirvShader& words, const Instruction& instruction)
    {
        const size_t at = instruction.m_at;
        const size_t end = at + instruction.m_count;
        bool out = false;
        switch (instruction.m_opcode)
        {
        case s_op_decorate_string:
        case s_op_member_decorate_string:
            out = true;
            break;
        case s_op_decorate_id:
            out = instruction.m_count >= 3 && words[at + 2] == s_decoration_hlsl_counter_buffer;
            break;
        case s_op_extension:
        {
            const std::string name = literal(words, at + 1, end);
            out = name == "SPV_GOOGLE_hlsl_functionality1" || name == "SPV_GOOGLE_user_type";
        }
        break;
        default:
            break;
        }
        return out;
    }

    //the stages of a program in the layout of Vulkan (false: a stage not read)
    static bool apply(TypeSpirvShaderList& shaders, const TargetShaderInfo& target_info, ErrorSpirvShaderList& errors)
    {
        std::vector<Stage> stages;
        stages.reserve(shaders.size());
        for (const TypeSpirvShader& shader : shaders)
        {
            stages.push_back(read(shader.m_shader));
            if (stages.back().m_instructions.empty())
            {
                errors.push_back("Vulkan layout: a stage is not SPIR-V");
                return false;
            }
        }
        //the bindings of the program: the blocks first, then the others, each name once
        std::unordered_map<std::string, unsigned int> bindings;
        unsigned int next = 0;
        for (bool blocks : { true, false })
        {
            for (const Stage& stage : stages)
            {
                for (const Resource& resource : stage.m_resources)
                {
                    if (resource.m_block == blocks && !bindings.count(resource.m_name))
                    {
                        bindings[resource.m_name] = next++;
                    }
                }
            }
        }
        //the locations of the inputs of the vertex stage, by their semantic
        std::unordered_map<std::string, unsigned int> locations;
        for (const auto& location : target_info.m_vertex_input_locations)
        {
            locations[semantic_with_index(std::string(location.first))] = location.second;
        }
        //each stage written again
        for (size_t i = 0; i != shaders.size(); ++i)
        {
            const SpirvShader& words = shaders[i].m_shader;
            Stage& stage = stages[i];
            SpirvShader out(words.begin(), words.begin() + s_header);
            out.reserve(words.size());
            //(the words of its decorations changed in the copy: by their position in it)
            std::unordered_map<size_t, unsigned int> changed;
            for (const Resource& resource : stage.m_resources)
            {
                if (stage.m_binding.count(resource.m_variable))
                {
                    changed[stage.m_binding[resource.m_variable]] = bindings[resource.m_name];
                }
                if (stage.m_set.count(resource.m_variable))
                {
                    changed[stage.m_set[resource.m_variable]] = 0;
                }
            }
            if (shaders[i].m_type == ST_VERTEX_SHADER)
            {
                //(its inputs only: its outputs keep the locations the next stage reads)
                for (unsigned int input : stage.m_inputs)
                {
                    const auto semantic = stage.m_semantics.find(input);
                    if (semantic == stage.m_semantics.end() || !stage.m_location.count(input)) continue;
                    const auto location = locations.find(semantic_with_index(semantic->second));
                    if (location != locations.end())
                    {
                        changed[stage.m_location[input]] = location->second;
                    }
                }
            }
            for (const Instruction& instruction : stage.m_instructions)
            {
                if (!removed(words, instruction))
                {
                    for (size_t w = instruction.m_at; w != instruction.m_at + instruction.m_count; ++w)
                    {
                        const auto change = changed.find(w);
                        out.push_back(change != changed.end() ? change->second : words[w]);
                    }
                }
            }
            shaders[i].m_shader = std::move(out);
        }
        return true;
    }
}

extern bool initialize()
{
    return glslang::InitializeProcess();
}

extern void shutdown()
{
    glslang::FinalizeProcess();
}

extern bool hlsl_to_spirv
(
      const String&                   hlsl_source
    , const String&                   hlsl_filename
    , const Vector<ShaderInfo>&       shaders_info
	, TypeSpirvShaderList&			  shaders_output
	, ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info
)
{
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
		//no entry point in the source: no parse
		if (!Parser::source_may_have_function(hlsl_source, shader_info.m_name)) continue;
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
			if (function == std::string_view(shader_info.m_name))
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
			//the SPIR-V of a driver of Vulkan: legalized (and optimized) by SPIRV-Tools, as the
			//driver reads it as it is; the other targets (SPIRV-Cross) keep the SPIR-V unchanged,
			//all its interface (e.g. the inputs of a fragment shader not read, as DirectX links
			//the stages by their signatures)
			spv_options.disableOptimizer = !target_info.m_vulkan_layout;
			spv_options.optimizeSize = false;
            // spv_options.validate = true;
			//shader output (the words of glslang into the ones of the interface)
			std::vector<unsigned int> output;
			//get output
			GlslangToSpv(*program.getIntermediate(shader_type), output, &logger, &spv_options);
			//build
            shaders_output[i].m_type = types[i];
            shaders_output[i].m_shader.assign(output.begin(), output.end());
        }
    }
    //the layout of a driver of Vulkan: the bindings of the program, the locations by semantic
    if (target_info.m_vulkan && target_info.m_vulkan_layout)
    {
        if (!VulkanLayout::apply(shaders_output, target_info, errors))
        {
            return false;
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
            if (function == std::string_view(shader_info.m_name))
            {
                return true;
            }
        }
    }
    return false;
}

extern bool hlsl_to_hlsl_preprocessed
(
      const String&                   hlsl_source
    , const String&                   hlsl_filename
    , const Vector<ShaderInfo>&       shaders_info
	, TypeHLSLShaderList&			  shaders_output
	, ErrorSpirvShaderList&			  errors
    , TargetShaderInfo			      target_info
    , bool                            remove_shader_wo_entrypoint
)
{
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
		//no entry point in the source: no preprocess/parse (when the shaders without it are removed)
		if (remove_shader_wo_entrypoint && !Parser::source_may_have_function(hlsl_source, shader_info.m_name)) continue;
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
        // Output (the string of glslang, into the one of the interface)
        std::string output_shader;
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
                        HLSLShader(output_shader) 
                    });
                }
            }
            else
            {
                // Add shader
                shaders_output.emplace_back(TypeHLSLShader{ 
                    glslang_type_to_render_shader_type(shader_type),
                    HLSLShader(output_shader) 
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
