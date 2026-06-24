//
//  SpirvToSource.cpp
//  Square
//
//  Created by Gabriele Di Bari on 25/01/18.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#include <cstring>
#include <cctype>
#include <regex>
#include <iterator>
#include <algorithm>
#include "HLSL2ALL/SpirvToSource.h"
#include "spirv.hpp"
#include "spirv_glsl.hpp"
#include "spirv_hlsl.hpp"
#include "spirv_msl.hpp"
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

static void replace_with_location(spirv_cross::Compiler& compiler, const std::string& prefix, const spirv_cross::SmallVector<spirv_cross::Resource>& resources)
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

static void add_sample_uniforms(spirv_cross::Compiler& compiler, std::string& output_source, const spirv_cross::SmallVector<spirv_cross::Resource>& resources)
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
    
static std::string delete_texture_query_levels(const std::string& in_source)
{
    //remove "#extension GL_ARB_texture_query_levels : require\n"
    //remove "uint param = uint(textureQueryLevels(SPIRV_Cross_Combinedtex2DSPIRV_Cross_DummySampler));\n"
    //regexs
    std::regex gl_arb_texture_query_levels_re("#extension(\\s)*GL_ARB_texture_query_levels(\\s)*\\:(\\s)require");
    std::regex texture_query_levels_re("uint\\(textureQueryLevels\\((\\w)*\\)\\)");
    //output
    std::string remove_gl_arg_texture_query;
    std::regex_replace (std::back_inserter(remove_gl_arg_texture_query), in_source.begin(), in_source.end(), gl_arb_texture_query_levels_re, "");
    std::string remove_texture_query_levels;
    std::regex_replace (std::back_inserter(remove_texture_query_levels), remove_gl_arg_texture_query.begin(), remove_gl_arg_texture_query.end(), texture_query_levels_re, "0");
    //return
    return remove_texture_query_levels;
}

static void force_uniform_buffer_binding_zero(spirv_cross::CompilerGLSL& compiler)
{
    auto active = compiler.get_active_interface_variables();
    auto resources = compiler.get_shader_resources(active);
    for (const auto& r : resources.uniform_buffers)
        compiler.set_decoration(r.id, spv::DecorationBinding, 0);
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
	if(config.m_rename_texture_mode != RenameTextureMode::FORCE_TO_ADD_SAMPLE_AS_TEXTURE)
	{
		glsl.build_dummy_sampler_for_combined_images();
		glsl.build_combined_image_samplers();
		//rename
		switch(config.m_rename_texture_mode)
		{
			default:
			case RenameTextureMode::USE_TEXTURE_NAME:
				for (auto &remap : glsl.get_combined_image_samplers())
				{
					glsl.set_name(remap.combined_id, glsl.get_name(remap.image_id));
				}
			break;
			case RenameTextureMode::COMBINE_TEXTURE_AND_SAMPLE:
				for (auto &remap : glsl.get_combined_image_samplers())
				{
					glsl.set_name(remap.combined_id, glsl.get_name(remap.image_id) +  glsl.get_name(remap.sampler_id));
				}
			break;
			case RenameTextureMode::RENAME_TEXTURE_WITH_SAMPLE:
				for (auto &remap : glsl.get_combined_image_samplers())
				{
					glsl.set_name(remap.combined_id, glsl.get_name(remap.sampler_id));
				}
			break;
		}
 	}
	//force all uniform-buffer bindings to 0 (host re-binds blocks by name)
	if(config.m_force_uniform_buffer_binding_zero)
	{
		force_uniform_buffer_binding_zero(glsl);
	}
    //compile
	source_glsl = glsl.compile();
	//force to add sample uniforms
	if(config.m_rename_texture_mode == RenameTextureMode::FORCE_TO_ADD_SAMPLE_AS_TEXTURE)
	{
    	auto active = glsl.get_active_interface_variables();
    	auto resources = glsl.get_shader_resources(active);
		add_sample_uniforms(glsl, source_glsl, resources.separate_samplers);
	}
    //force to remove query texture
    if(config.m_force_to_remove_query_texture)
    {
        source_glsl = delete_texture_query_levels(source_glsl);
    }
    //return
    return true;
}

//rename cbuffer 
static void replace_auto_cbuffer_names_with_source_names(spirv_cross::Compiler& compiler)
{
    //info
    auto active = compiler.get_active_interface_variables();
    spirv_cross::ShaderResources resources = compiler.get_shader_resources(active);
	//using name space
	using namespace spv;
	using namespace spirv_cross;
	using namespace spirv_cross_util;
    //replace
    for(const auto& r : resources.uniform_buffers)
    {
		//type		
		auto &type = compiler.get_type(r.base_type_id);
        //get info
		bool is_block = compiler.get_decoration_bitset(type.self).get(DecorationBlock) ||
						compiler.get_decoration_bitset(type.self).get(DecorationBufferBlock);
		//test
		if (is_block)
		{
        		std::string name = r.name;
				compiler.set_name(r.id, name);
		}
    }
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
    options.point_coord_compat = config.m_point_coord_compat;
    options.point_size_compat = config.m_point_size_compat;
	hlsl.set_hlsl_options(options);
	//force
	if(config.m_replace_auto_cbuffer_names_with_source_names)
		replace_auto_cbuffer_names_with_source_names(hlsl);
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

// MSL conversion with embedded reflection comment
// Reflection format (single header line):
//   // SQUARE_MTL_REFL entry:NAME textures:N=S[,N=S...] buffers:N=S[,...] members:N=BS:OFF:SZ[,...]
// textures  → name=slot
// buffers   → name=slot  (named cbuffers for get_uniform_const_buffer)
// members   → name=buffer_slot:byte_offset:byte_size  (loose uniforms inside a cbuffer)
bool spirv_to_msl
(
  SpirvShader shader
, std::string& source_msl
, ErrorSpirvShaderList& errors
, const MSLConfig& config
)
{
    SpirvShader spirv_binary = shader;
    spirv_cross::CompilerMSL msl(std::move(spirv_binary));

    spirv_cross::CompilerMSL::Options msl_opts;
    msl_opts.platform = config.m_ios
        ? spirv_cross::CompilerMSL::Options::iOS
        : spirv_cross::CompilerMSL::Options::macOS;
    msl_opts.msl_version = static_cast<uint32_t>(config.m_msl_version);
    msl_opts.use_fast_math_pragmas = config.m_optimize_shader;
    msl_opts.pad_argument_buffer_resources = true;
    // Do NOT set enable_decoration_binding — that would copy SPIR-V binding
    // indices verbatim into MSL [[buffer(N)]] attributes.  In HLSL/Vulkan
    // each stage has its own register namespace so two cbuffers in the same
    // stage can share slot 0 (e.g. Camera:b0 in both VS and PS).  MSL
    // requires unique indices per argument list, so we let SPIRV-Cross
    // auto-assign them and read them back via get_automatic_msl_resource_binding().
    msl.set_msl_options(msl_opts);

    // Common options
    spirv_cross::CompilerGLSL::Options common_opts = msl.get_common_options();
    common_opts.vertex.fixup_clipspace = config.m_fixup_clipspace;
    common_opts.force_zero_initialized_variables = config.m_optimize_shader;
    common_opts.fragment.default_float_precision = config.m_optimize_shader 
                                                 ? spirv_cross::CompilerGLSL::Options::Mediump
                                                 : spirv_cross::CompilerGLSL::Options::Highp;
    common_opts.fragment.default_int_precision = spirv_cross::CompilerGLSL::Options::Highp;
    msl.set_common_options(common_opts);

    // Rename entry points whose names are reserved MSL stage qualifiers or
    // whose names match the fixed entry-point names used by this project's
    // HLSL→SPIR-V pipeline (see Shader.cpp shader_target_name[]).
    // SPIRV-Cross would otherwise emit e.g. "vertex ReturnType vertex(...)"
    // which the Metal compiler rejects because "vertex" is a keyword.
    // Reserved MSL qualifiers: vertex, fragment, kernel.
    // Project entry-point names: vertex, fragment, geometry, tass_control,
    //                            tass_eval, compute (mapped to "kernel" in MSL).
    {
        static const std::string k_msl_reserved[] =
        {
            "vertex", "fragment", "kernel",   // MSL stage-qualifier keywords
            "geometry",                        // not an MSL keyword but unsupported in Metal
            "tass_control", "tass_eval",       // tessellation stages
            "compute",                         // our name for the compute stage
        };
        auto eps = msl.get_entry_points_and_stages();
        for (const auto& ep : eps)
        {
            for (const auto& kw : k_msl_reserved)
            {
                if (ep.name == kw)
                {
                    msl.rename_entry_point(ep.name, ep.name + "_main", ep.execution_model);
                    break;
                }
            }
        }
    }

    std::string msl_source;
    try
    {
        msl_source = msl.compile();
    }
    catch (std::exception& e)
    {
        errors.push_back(e.what());
        return false;
    }

    // Normalize Metal fast-math intrinsics to their precise/plain equivalents so
    // the MSL output matches the GLSL/HLSL backends. Recent SPIRV-Cross emits
    // fast::normalize / fast::min / fast::max / fast::clamp and powr() by default
    // (the entry point has no SignedZeroInfNanPreserve mode, so it assumes full
    // fast-math). On Apple GPUs fast:: flushes denormals and has relaxed/UB
    // behaviour on NaN/Inf, which diverges from OpenGL/DirectX (plain pow/normalize)
    // and produces artefacts (e.g. PBR lighting blowing out to the light colour).
    // A targeted string replace keeps the change inside our pipeline.
    /* if (!config.m_optimize_shader) */
    {
        auto replace_all_str = [](std::string& s, const std::string& from, const std::string& to)
        {
            for (std::string::size_type i = 0; (i = s.find(from, i)) != std::string::npos; i += to.size())
                s.replace(i, from.size(), to);
        };
        replace_all_str(msl_source, "fast::", "");   // fast::normalize -> normalize, fast::max -> max, ...
        replace_all_str(msl_source, "powr(",  "pow("); // powr requires base>=0 (NaN otherwise) -> use pow
    }

    // ── Build reflection comment ──────────────────────────────────────────
    auto active = msl.get_active_interface_variables();
    spirv_cross::ShaderResources res = msl.get_shader_resources(active);

    // Entry point
    auto entry_points = msl.get_entry_points_and_stages();
    std::string entry_name = entry_points.empty() ? "main0" : entry_points[0].name;

    // Collect textures (separate images and combined samplers)
    std::string tex_part, samp_part, buf_part, mem_part;

    auto append_kv = [](std::string& out, const std::string& name, uint32_t slot)
    {
        if (!out.empty()) out += ',';
        out += name + '=' + std::to_string(slot);
    };

    for (const auto& r : res.separate_images)
    {
        uint32_t slot = msl.get_automatic_msl_resource_binding(r.id);
        if (slot != uint32_t(-1)) append_kv(tex_part, r.name, slot);
    }
    for (const auto& r : res.sampled_images)
    {
        uint32_t slot = msl.get_automatic_msl_resource_binding(r.id);
        if (slot != uint32_t(-1)) append_kv(tex_part, r.name, slot);
    }
    for (const auto& r : res.separate_samplers)
    {
        uint32_t slot = msl.get_automatic_msl_resource_binding(r.id);
        if (slot != uint32_t(-1)) append_kv(samp_part, r.name, slot);
    }

    // Uniform buffers: named cbuffers → buf_part; walk members → mem_part
    for (const auto& r : res.uniform_buffers)
    {
        uint32_t bslot = msl.get_automatic_msl_resource_binding(r.id);
        if (bslot == uint32_t(-1)) continue;
        append_kv(buf_part, r.name, bslot);

        // Only the implicit globals cbuffer ($Globals -> _Global/_Globals) holds
        // loose uniforms. Members of *named* cbuffers (Light, Camera, Transform…)
        // are accessed through the bound constant buffer, never as loose uniforms,
        // so they must NOT go into the members list — otherwise the Metal backend
        // folds them into its auto/_Global buffer and can bind that buffer over a
        // named cbuffer's slot (corrupting e.g. the point-light data → red blow-out).
        if (r.name.find("Global") == std::string::npos)
            continue;

        // Walk struct members for loose-uniform access
        const spirv_cross::SPIRType& btype = msl.get_type(r.base_type_id);
        for (uint32_t mi = 0; mi < btype.member_types.size(); ++mi)
        {
            std::string mname = msl.get_member_name(r.base_type_id, mi);
            if (mname.empty()) continue;
            uint32_t moffset = msl.type_struct_member_offset(btype, mi);
            size_t   msize   = msl.get_declared_struct_member_size(btype, mi);
            if (!mem_part.empty()) mem_part += ',';
            mem_part += mname + '=' + std::to_string(bslot)
                      + ':' + std::to_string(moffset)
                      + ':' + std::to_string(msize);
        }
    }

    // Assemble header comment
    std::string refl = "// SQUARE_MTL_REFL";
    refl += " entry:" + entry_name;
    if (!tex_part.empty())  refl += " textures:"  + tex_part;
    if (!samp_part.empty()) refl += " samplers:"  + samp_part;
    if (!buf_part.empty())  refl += " buffers:"   + buf_part;
    if (!mem_part.empty())  refl += " members:"   + mem_part;
    refl += '\n';

    source_msl = refl + msl_source;
    return true;
}

}
