#include "stdafx.h"
#include "r2.h"

#include "Layers/xrRender/ShaderResourceTraits.h"
#include "xrCore/FileCRC32.h"
#include "Layers/xrRender/r_constants.h"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <spirv_cross_c.h>

namespace xray::render::RENDER_NAMESPACE
{
xr_map<u32, void*> s_shaderFuncs;
u32 s_nextShaderID = 1;

void register_shader_func(void* func, u32& outID)
{
    outID = s_nextShaderID++;
    s_shaderFuncs[outID] = func;
}
}

namespace xray::render::RENDER_NAMESPACE
{
void setup_constants_from_msl(R_constant_table& table, pcstr mslSource, u32 destination);

static void patch_io_locations(xr_string& s);
static void patch_alpha_swizzle(xr_string& src);
static void strip_texel_offsets(xr_string& src);

void* lookup_shader_func(u32 id)
{
    auto it = s_shaderFuncs.find(id);
    return it != s_shaderFuncs.end() ? it->second : nullptr;
}

static bool s_glslangInitialized = false;

static void ensure_glslang()
{
    if (!s_glslangInitialized)
    {
        s_glslangInitialized = glslang_initialize_process() != 0;
    }
}

// --- glslang include callbacks ---
static glsl_include_result_t* include_local(void* ctx, const char* header_name,
    const char* includer_name, size_t include_depth)
{
    xr_string header(header_name);
    // Normalize forward slashes to backslashes (engine convention)
    for (auto& c : header)
        if (c == '/') c = '\\';

    // Construct relative path: "mtl\" + header
    string_path relPath;
    strconcat(sizeof(relPath), relPath, RImplementation.getShaderPath(), header.c_str());
    // Open via $game_shaders$ alias
    IReader* reader = FS.r_open("$game_shaders$", relPath);
    if (reader)
    {
        size_t size = reader->length();
        char* data = (char*)malloc(size + 1);
        CopyMemory(data, reader->pointer(), size);
        data[size] = '\0';
        FS.r_close(reader);
        // Normalize backslashes to forward slashes in included content
        for (size_t i = 0; i < size; i++)
            if (data[i] == '\\') data[i] = '/';
        // Strip texel offsets (non-const offets in *Offset() calls)
        {
            xr_string stripped(data);
            strip_texel_offsets(stripped);
            patch_io_locations(stripped);
            xr_free(data);
            size = stripped.size();
            // Ensure content ends with newline (glslang doesn't add one)
            // to prevent #endifuniform bug when the next line is concatenated
            bool needsNewline = (size == 0 || stripped.back() != '\n');
            data = (char*)malloc(size + 1 + (needsNewline ? 1 : 0));
            CopyMemory(data, stripped.c_str(), size);
            if (needsNewline)
            {
                data[size] = '\n';
                size++;
            }
            data[size] = '\0';
        }
        auto* result = (glsl_include_result_t*)malloc(sizeof(glsl_include_result_t));
        result->header_name = strdup(header_name);
        result->header_data = data;
        result->header_length = size;
        return result;
    }
    Msg("! include_local: NOT FOUND [%s] depth=%zu from=[%s]", relPath, include_depth, includer_name);
    return nullptr;
}

static glsl_include_result_t* include_system(void* ctx, const char* header_name,
    const char* includer_name, size_t include_depth)
{
    return include_local(ctx, header_name, includer_name, include_depth);
}

static int free_include_result(void* ctx, glsl_include_result_t* result)
{
    if (result)
    {
        if (result->header_name)
            free((void*)result->header_name);
        if (result->header_data)
            free((void*)result->header_data);
        free(result);
    }
    return 0;
}

static void add_output_locations(xr_string& source, glslang_stage_t stage)
{
    if (stage != GLSLANG_STAGE_VERTEX)
        return;

    // Find the last closing brace of the _main function output assignment
    size_t pos = source.rfind("}");

    // Pad output variables: for vertex shaders used with point sprites, add
    // a dummy PointSize output to satisfy SPIRV-Cross MSL requirements.
    // Vertex outputs at location 0 and 8 are already defined in p_TL.h etc.
    // We just need to ensure location(1) is assigned for the gl_PointSize.
    if (pos != xr_string::npos && pos > 0)
    {
        // Insert at the end of the function body but before the closing brace
        // SPIRV-Cross needs to know gl_PointSize is at location 1
        // We'll rely on the existing output struct having the right layout
    }
}

static void strip_texel_offsets(xr_string& src)
{
    // SPIR-V/MSL requires offset arguments to texture* functions to be
    // compile-time constants. The engine sometimes passes non-const offsets,
    // so strip them by removing the "Offset" suffix and the last argument.
    //
    // textureOffset(sampler, coord, offset)       → texture(sampler, coord)
    // textureGatherOffset(sampler, coord, offset) → textureGather(sampler, coord)
    // texelFetchOffset(sampler, coord, lod, offset) → texelFetch(sampler, coord, lod)
    // textureLodOffset(sampler, coord, lod, offset) → textureLod(sampler, coord, lod)

    static const char* patterns[] = {
        "textureOffset(",
        "textureGatherOffset(",
        "texelFetchOffset(",
        "textureLodOffset(",
    };

    for (auto* pat : patterns)
    {
        size_t patLen = strlen(pat); // e.g. "textureOffset(" = 15
        size_t pos = 0;
        while ((pos = src.find(pat, pos)) != xr_string::npos)
        {
            // Remove "Offset" suffix (6 chars before the paren, "Offset(" is last 7 chars)
            src.erase(pos + patLen - 7, 6);
            // Now patLen = old patLen - 6, but we don't need it anymore
            // Remove the LAST argument (the offset — always the last arg)
            size_t paren = src.find('(', pos);
            if (paren == xr_string::npos) { pos++; continue; }
            // Find the last comma inside the parens
            int depth = 1;
            size_t lastComma = xr_string::npos;
            size_t end = paren + 1;
            while (end < src.size() && depth > 0)
            {
                if (src[end] == '(') depth++;
                else if (src[end] == ')') { if (--depth == 0) break; }
                else if (src[end] == ',' && depth == 1) lastComma = end;
                end++;
            }
            if (depth == 0 && lastComma != xr_string::npos)
            {
                // Remove from lastComma to closing paren
                src.erase(lastComma, end - lastComma);
            }
            pos = paren + 1;
        }
    }
}

static void patch_io_locations(xr_string& s)
{
    // SPIR-V requires explicit layout(location=...) for all IO variables.
    // Fragment outputs use SV_TargetN convention where N is the render target index.
    // The VS/PS IO struct members already use #define-based locations
    // (COLOR=0, TEXCOORD0=8, etc.) from common.h.
    // We patch "out vec4 SV_Target[N]" → "layout(location=N) out vec4 SV_Target[N]"
    size_t pos = 0;
    const char needle[] = "out vec4 SV_Target";
    const size_t needleLen = sizeof(needle) - 1;
    while ((pos = s.find(needle, pos)) != xr_string::npos)
    {
        // Check if it already has a layout qualifier on the same line
        size_t lineStart = s.rfind('\n', pos);
        if (lineStart == xr_string::npos) lineStart = 0;
        else lineStart++;
        xr_string prefix = s.substr(lineStart, pos - lineStart);
        if (prefix.find("layout") != xr_string::npos)
        {
            pos += needleLen;
            continue;
        }
        // Determine location number from the suffix after "SV_Target"
        // SV_Target   → loc 0 (followed by ';' or whitespace)
        // SV_Target0  → loc 0
        // SV_Target1  → loc 1
        // SV_Target2  → loc 2
        // SV_TargetN  → loc N
        int loc = 0;
        size_t after = pos + needleLen;
        if (after < s.size() && s[after] >= '0' && s[after] <= '9')
        {
            loc = s[after] - '0';
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "layout(location=%d) ", loc);
        s.insert(pos, buf);
        pos += strlen(buf);
    }
}

static xr_string resolve_includes(const xr_string& source, int depth = 0)
{
    if (depth > 20)
        return source;

    // Track preprocessor conditional depth (#if, #ifdef, #ifndef → #endif)
    // Only resolve includes at depth 0 (unconditional)
    int ifDepth = 0;

    xr_string result;
    size_t pos = 0;
    size_t len = source.size();

    while (pos < len)
    {
        size_t hashPos = source.find('#', pos);
        if (hashPos == xr_string::npos || hashPos + 9 > len)
            break;

        // Extract the directive text
        size_t lineEnd = source.find('\n', hashPos);
        if (lineEnd == xr_string::npos) lineEnd = len;
        xr_string line = source.substr(hashPos, lineEnd - hashPos);

        // Track conditional depth
        if (line.size() >= 2)
        {
            // Skip whitespace after #
            size_t dp = 1;
            while (dp < line.size() && (line[dp] == ' ' || line[dp] == '\t')) dp++;
            xr_string directive = line.substr(dp);

            if (directive.size() >= 2 && directive.substr(0, 2) == "if")
                ifDepth++;
            else if (directive.size() >= 5 && directive.substr(0, 5) == "endif")
            {
                if (ifDepth > 0) ifDepth--;
            }
            else if (ifDepth == 0 && directive.size() >= 7 && directive.substr(0, 7) == "include")
            {
                // Unconditional include — resolve it
                result += source.substr(pos, hashPos - pos);

                // Find the header name between quotes
                size_t q1 = line.find('"', dp + 7);
                if (q1 != xr_string::npos)
                {
                    size_t q2 = line.find('"', q1 + 1);
                    if (q2 != xr_string::npos)
                    {
                        xr_string headerName = line.substr(q1 + 1, q2 - q1 - 1);

                        // Normalize forward slashes to backslashes
                        for (auto& c : headerName)
                            if (c == '/') c = '\\';

                        string_path relPath;
                        strconcat(sizeof(relPath), relPath,
                            RImplementation.getShaderPath(), headerName.c_str());

                        IReader* reader = FS.r_open("$game_shaders$", relPath);
                        if (reader)
                        {
                            xr_string content(
                                static_cast<const char*>(reader->pointer()),
                                reader->length());
                            FS.r_close(reader);

                            for (auto& c : content)
                                if (c == '\\') c = '/';

                            content = resolve_includes(content, depth + 1);
                            // Ensure included content ends with a newline
                            // to prevent concatenation with the next line (#endifuniform bug)
                            if (!content.empty() && content.back() != '\n')
                                content += '\n';
                            result += content;
                        }

                        pos = lineEnd + 1;
                        continue;
                    }
                }
            }
        }

        // Move to next line
        result += source.substr(pos, lineEnd - pos);
        if (lineEnd < len)
            result += '\n';
        pos = lineEnd + 1;
    }

    if (pos < len)
        result += source.substr(pos);
    return result;
}

static void patch_alpha_swizzle(xr_string& src)
{
    // Metal's A8Unorm pixel format stores single-channel data in the alpha
    // component. GLSL `.rrrr` swizzle reads from the red channel (which is 0
    // for A8 textures), making fonts invisible. Replace `.rrrr` → `.aaaa`.
    size_t pos = 0;
    while ((pos = src.find(".rrrr", pos)) != xr_string::npos)
    {
        src.replace(pos, 5, ".aaaa");
        pos += 5;
    }
}

static xr_string build_full_glsl(pcstr pTarget, pcstr resolvedSource, pcstr shaderOptions)
{
    xr_string src;
    src += "#version 420\n";
    src += "#extension GL_GOOGLE_include_directive : enable\n";
    if (shaderOptions && shaderOptions[0])
        src += shaderOptions;
    src += resolvedSource;
    // Pre-resolve #include directives manually (bypasses glslang include callbacks)
    src = resolve_includes(src);
    strip_texel_offsets(src);
    patch_io_locations(src);
    // Normalize backslashes to forward slashes in any remaining #include paths
    // (glslang's preprocessor interprets \v, \s, etc. as escape sequences).
    {
        size_t pos = 0;
        while ((pos = src.find("#include", pos)) != xr_string::npos)
        {
            size_t q1 = src.find('"', pos + 8);
            if (q1 != xr_string::npos)
            {
                size_t q2 = src.find('"', q1 + 1);
                if (q2 != xr_string::npos)
                {
                    for (size_t i = q1 + 1; i < q2; ++i)
                        if (src[i] == '\\') src[i] = '/';
                }
            }
            pos = pos + 8;
        }
    }
    // Fix float4→float3 type mismatch for SKIN_NONE (no SKIN_0) vertex shaders.
    // v_model_*.h uses #ifdef SKIN_0 to pick float3 vs float4 for NORMAL.
    // With only SKIN_NONE defined, v_model_N is float4 but v_model.N is float3.
    if (pTarget[0] == 'v' &&
        src.find("#define SKIN_NONE 1") != xr_string::npos &&
        src.find("#define SKIN_0 1") == xr_string::npos)
    {
        size_t pos = 0;
        while ((pos = src.find("I.N = v_model_N", pos)) != xr_string::npos)
        {
            src.replace(pos, 15, "I.N = v_model_N.xyz");
            pos += 19;
        }
    }

    if (pTarget[0] == 'p')
        patch_alpha_swizzle(src);
    return src;
}

void CRender::addShaderOption(const char* name, const char* value)
{
    m_ShaderOptions += "#define ";
    m_ShaderOptions += name;
    m_ShaderOptions += " ";
    m_ShaderOptions += value;
    m_ShaderOptions += "\n";
}

static bool glsl_to_spirv(const char* glslSource, glslang_stage_t stage, xr_vector<unsigned int>& spirv);
static bool spirv_to_msl(const unsigned int* spirv, size_t word_count, xr_string& outMSL, bool isVertex);
static bool compile_msl(MTL::Device* device, const char* source, const char* entryPoint, MTL::Function*& outFunction);
static glslang_stage_t target_to_stage(pcstr pTarget);

static u32 create_shader(pcstr pTarget, void*& result, pcstr resolvedSource, pcstr shaderOptions)
{
    auto* device = static_cast<MTL::Device*>(HW.m_device);

    // Build full GLSL with #version 420 + options
    xr_string fullGLSL;
    try
    {
        fullGLSL = build_full_glsl(pTarget, resolvedSource, shaderOptions);
    }
    catch (const std::exception& e)
    {
        Msg("! create_shader build_full_glsl exception: %s target=%s", e.what(), pTarget);
        return 0;
    }

    // GLSL → SPIR-V
    glslang_stage_t stage = target_to_stage(pTarget);
    xr_vector<unsigned int> spirv;
    if (!glsl_to_spirv(fullGLSL.c_str(), stage, spirv))
    {
        return 0;
    }

    // SPIR-V → MSL
    xr_string mslSource;
    if (!spirv_to_msl(spirv.data(), spirv.size(), mslSource, pTarget[0] == 'v'))
    {
        Msg("! Failed to convert SPIR-V to MSL (target=%s)", pTarget);
        return 0;
    }

    // Populate constant table from MSL (active uniforms only)
    auto* shVS = (pTarget[0] == 'v') ? (SVS*&)result : nullptr;
    auto* shPS = (pTarget[0] == 'p') ? (SPS*&)result : nullptr;
    if (shVS || shPS)
    {
        setup_constants_from_msl(
            (shVS ? shVS->constants : shPS->constants),
            mslSource.c_str(),
            (shVS ? RC_dest_vertex : RC_dest_pixel)
        );
    }

    // MSL → Metal function
    // SPIRV-Cross generates "main0" as entry point name for vertex/fragment shaders
    MTL::Function* func = nullptr;
    if (!compile_msl(device, mslSource.c_str(), "main0", func))
    {
        Msg("! Failed to compile MSL shader (target=%s)", pTarget);
        return 0;
    }

    if (!func)
        return 0;

    if (pTarget[0] == 'p')
    {
        auto* sh = (SPS*&)result;
        u32 id;
        register_shader_func(func, id);
        sh->sh = id;
        sh->constants.parse(resolvedSource ? const_cast<pstr>(resolvedSource) : nullptr, RC_dest_pixel);
        // Re-apply MSL bindings AFTER parse() to override pre-seeded GL-style indices
        setup_constants_from_msl(sh->constants, mslSource.c_str(), RC_dest_pixel);
    }
    else if (pTarget[0] == 'v')
    {
        auto* sh = (SVS*&)result;
        u32 id;
        register_shader_func(func, id);
        sh->sh = id;
        sh->constants.parse(resolvedSource ? const_cast<pstr>(resolvedSource) : nullptr, RC_dest_vertex);
        // Re-apply MSL bindings AFTER parse() to override pre-seeded GL-style indices
        setup_constants_from_msl(sh->constants, mslSource.c_str(), RC_dest_vertex);
    }
    else
        return 0;

    return 1;
}

HRESULT CRender::shader_compile(pcstr name, IReader* fs, pcstr pFunctionName,
    pcstr pTarget, u32 Flags, void*& result)
{
    // Load raw source (with #include directives) — glslang's include callbacks handle resolution
    xr_string source(static_cast<const char*>(fs->pointer()), fs->length());

    // Build per-shader prefix: common options + SKIN macro
    // Matches options set by the GL backend in shader_sources_manager::Apply()
    xr_string prefix;

    // Shadow map size
    char smap_size_str[16];
    xr_itoa(m_SMAPSize, smap_size_str, 10);
    prefix += "#define SMAP_size " + xr_string(smap_size_str) + "\n";

    // FP16 filter/blend
    if (o.fp16_filter)
        prefix += "#define FP16_FILTER 1\n";
    if (o.fp16_blend)
        prefix += "#define FP16_BLEND 1\n";

    // Hardware shadow map support
    if (o.HW_smap)
        prefix += "#define USE_HWSMAP 1\n";
    if (o.HW_smap_PCF)
        prefix += "#define USE_HWSMAP_PCF 1\n";
    if (o.HW_smap_FETCH4)
        prefix += "#define USE_FETCH4 1\n";

    // SJitter
    if (o.sjitter)
        prefix += "#define USE_SJITTER 1\n";

    // Per-shader SKIN macro (m_skinning is set by shader_option_skinning()
    // in SkeletonX.cpp before compilation, matching the GL/DX11 backends).
    // Note: We do NOT define SKIN_0 alongside SKIN_NONE here, even though
    // v_model_*.h needs SKIN_0 to use float3 v_model_N (matching v_model.N).
    // Defining SKIN_0 pulls in sbones_array from skin.h (234 float4 uniforms)
    // which exceeds Metal's 31-buffer limit for unused bindings. Instead,
    // the float4→float3 type mismatch is fixed at the source level in
    // build_full_glsl() by adding .xyz truncation to I.N = v_model_N.
    if (m_skinning < 0)
        prefix += "#define SKIN_NONE 1\n";
    else if (m_skinning == 0)
        prefix += "#define SKIN_0 1\n";
    else if (m_skinning == 1)
        prefix += "#define SKIN_1 1\n";
    else if (m_skinning == 2)
        prefix += "#define SKIN_2 1\n";
    else if (m_skinning == 3)
        prefix += "#define SKIN_3 1\n";
    else if (m_skinning == 4)
        prefix += "#define SKIN_4 1\n";
    else
    {
        Msg("! shader_compile: m_skinning=%d (out of range) for '%s' target=%s, defaulting to SKIN_NONE",
            (int)m_skinning, name, pTarget);
        prefix += "#define SKIN_NONE 1\n";
    }

    source = prefix + source;

    create_shader(pTarget, result, source.c_str(), m_ShaderOptions.c_str());
    return S_OK;
}

static bool glsl_to_spirv(const char* glslSource, glslang_stage_t stage, xr_vector<unsigned int>& spirv)
{
    ensure_glslang();
    if (!s_glslangInitialized)
        return false;

    xr_string source(glslSource);
    add_output_locations(source, stage);

    glslang_input_t input{};
    input.language = GLSLANG_SOURCE_GLSL;
    input.stage = stage;
    input.client = GLSLANG_CLIENT_NONE;
    input.client_version = GLSLANG_TARGET_OPENGL_450;
    input.target_language = GLSLANG_TARGET_SPV;
    input.target_language_version = GLSLANG_TARGET_SPV_1_3;
    input.code = source.c_str();
    input.default_version = 420;
    input.default_profile = GLSLANG_NO_PROFILE;
    input.force_default_version_and_profile = 0;
    input.forward_compatible = 1;
    input.messages = GLSLANG_MSG_DEFAULT_BIT;
    input.resource = glslang_default_resource();
    input.callbacks.include_local = include_local;
    input.callbacks.include_system = include_system;
    input.callbacks.free_include_result = free_include_result;

    glslang_shader_t* shader = glslang_shader_create(&input);
    if (!shader)
        return false;

    if (!glslang_shader_preprocess(shader, &input))
    {
        Msg("! glslang preprocess error: %s", glslang_shader_get_info_log(shader));
        glslang_shader_delete(shader);
        return false;
    }

    if (!glslang_shader_parse(shader, &input))
    {
        Msg("! glslang parse error: %s", glslang_shader_get_info_log(shader));
        // Dump first 30 lines of GLSL source for debugging
        xr_string src(input.code);
        size_t nlCount = 0, pos = 0;
        Msg("--- GLSL source (first 30 lines, %zu total bytes) ---", src.size());
        while (nlCount < 30 && pos < src.size())
        {
            size_t end = src.find('\n', pos);
            if (end == xr_string::npos)
                end = src.size();
            Msg("%s", xr_string(src.c_str() + pos, end - pos).c_str());
            pos = end + 1;
            nlCount++;
        }
        if (pos < src.size())
            Msg("... (%zu more bytes)", src.size() - pos);
        glslang_shader_delete(shader);
        return false;
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_DEFAULT_BIT))
    {
        Msg("! glslang link error: %s", glslang_program_get_info_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return false;
    }

    glslang_program_SPIRV_generate(program, stage);

    size_t count = glslang_program_SPIRV_get_size(program);
    spirv.resize(count);
    glslang_program_SPIRV_get(program, spirv.data());

    glslang_program_delete(program);
    glslang_shader_delete(shader);
    return true;
}

// Global consistent buffer index mapping across all shaders (per stage)
// Different shaders may assign different [[buffer(N)]] to the same named constant
// (e.g. c_brightness is buffer 0 in postprocess.ps but buffer 1 in postprocess_cm.ps).
// We remap all shaders to use the same global index per constant name.
static void remap_msl_buffers(xr_string& msl, bool isVertex)
{
    // Safety pass: ensure no [[buffer(N)]] has N >= 30 (Metal's limit).
    // With 0-based SPIR-V bindings per resource type, this should be a no-op,
    // but handle edge cases like storage buffers or argument buffers we don't explicitly bind.
    unsigned nextBuffer = 0;

    const char* p = msl.c_str();
    while ((p = strstr(p, "[[buffer(")) != nullptr)
    {
        const char* numStart = p + 9;
        char* endp = nullptr;
        unsigned oldIdx = (unsigned)strtoul(numStart, &endp, 10);
        if (oldIdx >= 30)
        {
            char oldBuf[32], newBuf[32];
            snprintf(oldBuf, sizeof(oldBuf), "[[buffer(%u)]]", oldIdx);
            snprintf(newBuf, sizeof(newBuf), "[[buffer(%u)]]", nextBuffer++);
            size_t rp = 0;
            size_t afterLast = 0;
            while ((rp = msl.find(oldBuf, rp)) != xr_string::npos)
            {
                msl.replace(rp, strlen(oldBuf), newBuf);
                rp += strlen(newBuf);
                afterLast = rp;
            }
            p = afterLast > 0 ? msl.c_str() + afterLast : nullptr;
        }
        else
        {
            nextBuffer = oldIdx + 1;
            p += 10;
        }
    }
}

static void fix_msl_params(xr_string& msl)
{
    // Fix common SPIRV-Cross MSL issues:
    // 1. Change "thread const texture2d<float>&" to just "texture2d<float>" (Metal texture params are by value)
    // 2. Change "thread const sampler&" to just "sampler"
    size_t pos = 0;
    while ((pos = msl.find("thread const texture", pos)) != xr_string::npos)
    {
        size_t end = pos;
        while (end < msl.size() && msl[end] != '>')
            ++end;
        if (end < msl.size())
            ++end;
        msl.erase(pos, end - pos);
        msl.insert(pos, "texture");
        pos += 7;
    }
    pos = 0;
    while ((pos = msl.find("thread const sampler&", pos)) != xr_string::npos)
    {
        msl.erase(pos, 21);
        msl.insert(pos, "sampler");
        pos += 7;
    }
    // 3. SPIRV-Cross generates "thread const floatNxM&" for uniform matrix/vector
    //    parameters in static helper functions, but the caller passes "constant floatNxM&"
    //    from buffer bindings. Metal requires matching address spaces.
    //    Fix: remove the reference (&) so the parameter is passed by value.
    //    This eliminates the address space conflict and generates equivalent code
    //    since these types are small (16-64 bytes).
    pos = 0;
    while ((pos = msl.find("thread const float", pos)) != xr_string::npos)
    {
        size_t t = pos + 18; // after "thread const float" (18 chars)
        if (t >= msl.size()) { pos++; continue; }
        // Check for matrix type: floatNxM (N,M=2-4)
        bool isMatrix = (msl[t] >= '2' && msl[t] <= '4' && t + 2 < msl.size() &&
                         msl[t+1] == 'x' && msl[t+2] >= '2' && msl[t+2] <= '4');
        // Check for vector type: float2, float3, float4 (without x)
        bool isVector = (msl[t] >= '2' && msl[t] <= '4' &&
                         (t + 1 >= msl.size() || msl[t+1] != 'x')) &&
                        (t + 1 < msl.size() && (msl[t+1] == ' ' || msl[t+1] == '&'));
        if (isMatrix || isVector)
        {
            // Find the & after the type name (matrix="4x4"=3 chars, vector="4"=1 char)
            size_t amp = t + (isMatrix ? 3 : 1);
            while (amp < msl.size() && msl[amp] == ' ') amp++;
            if (amp < msl.size() && msl[amp] == '&')
            {
                // Remove '&' to make it pass-by-value
                msl.erase(amp, 1);
                // Also need to remove 'thread ' before the parameter — only if this
                // parameter actually has 'thread'. But the 'thread' qualifier is
                // redundant for by-value params. Remove it.
                // 'thread ' starts at pos:
                msl.erase(pos, 7); // remove "thread "
                pos += 11; // sizeof("const float...")
                continue;
            }
        }
        pos++;
    }
    // 4. Fix "thread const spvUnsafeArray<T,N>&" for array params from constant buffers
    pos = 0;
    while ((pos = msl.find("thread const spvUnsafeArray<", pos)) != xr_string::npos)
    {
        size_t closeBracket = pos + 28;
        int depth = 1;
        while (closeBracket < msl.size() && depth > 0)
        {
            if (msl[closeBracket] == '<') depth++;
            else if (msl[closeBracket] == '>') depth--;
            closeBracket++;
        }
        if (depth == 0)
        {
            size_t amp = closeBracket;
            while (amp < msl.size() && msl[amp] == ' ') amp++;
            if (amp < msl.size() && msl[amp] == '&')
                msl.erase(amp, 1);
            msl.erase(pos, 7);
        }
        else
            pos++;
    }
}

static bool spirv_to_msl(const unsigned int* spirv, size_t word_count, xr_string& outMSL, bool isVertex)
{
    spvc_context ctx{};
    if (spvc_context_create(&ctx) != SPVC_SUCCESS)
        return false;

    spvc_parsed_ir ir{};
    if (spvc_context_parse_spirv(ctx, spirv, word_count, &ir) != SPVC_SUCCESS)
    {
        spvc_context_destroy(ctx);
        return false;
    }

    spvc_compiler compiler{};
    if (spvc_context_create_compiler(ctx, SPVC_BACKEND_MSL, ir, SPVC_CAPTURE_MODE_COPY, &compiler) != SPVC_SUCCESS)
    {
        spvc_context_destroy(ctx);
        return false;
    }

    {
        spvc_compiler_options opts{};
        if (spvc_compiler_create_compiler_options(compiler, &opts) == SPVC_SUCCESS)
        {
            spvc_compiler_options_set_uint(opts, SPVC_COMPILER_OPTION_MSL_PLATFORM, 2);
            spvc_compiler_options_set_uint(opts, SPVC_COMPILER_OPTION_MSL_VERSION, 30000);
            spvc_compiler_options_set_bool(opts, SPVC_COMPILER_OPTION_MSL_PAD_FRAGMENT_OUTPUT_COMPONENTS, true);
            spvc_compiler_options_set_bool(opts, SPVC_COMPILER_OPTION_FLIP_VERTEX_Y, true);
            spvc_compiler_install_compiler_options(compiler, opts);
        }
    }

    {
        spvc_resources resources{};
        if (spvc_compiler_create_shader_resources(compiler, &resources) == SPVC_SUCCESS)
        {
            const spvc_reflected_resource* list = nullptr;
            size_t count = 0;

            if (!isVertex)
            {
                spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &list, &count);
                for (size_t i = 0; i < count; i++)
                    spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationLocation, (unsigned)i);
            }

            spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &list, &count);
            for (size_t i = 0; i < count; i++)
                spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationBinding, (unsigned)i);

            spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);
            for (size_t i = 0; i < count; i++)
                spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationBinding, (unsigned)i);
        }
    }

    const char* source = nullptr;
    if (spvc_compiler_compile(compiler, &source) != SPVC_SUCCESS)
    {
        const char* err = spvc_context_get_last_error_string(ctx);
        if (!err) err = "unknown error";
        Msg("! SPIRV-Cross MSL compile error: %s", err);
        spvc_context_destroy(ctx);
        return false;
    }

    outMSL = source;
    fix_msl_params(outMSL);
    remap_msl_buffers(outMSL, isVertex);

    spvc_context_destroy(ctx);
    return true;
}

static glslang_stage_t target_to_stage(pcstr pTarget)
{
    if (pTarget[0] == 'v')
        return GLSLANG_STAGE_VERTEX;
    if (pTarget[0] == 'p')
        return GLSLANG_STAGE_FRAGMENT;
    if (pTarget[0] == 'g')
        return GLSLANG_STAGE_GEOMETRY;
    return GLSLANG_STAGE_VERTEX;
}

static bool compile_msl(MTL::Device* device, const char* source, const char* entryPoint, MTL::Function*& outFunction)
{
    if (!device || !source || !entryPoint)
        return false;

    NS::Error* error = nullptr;
    MTL::Library* library = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), nullptr, &error);

    if (!library)
    {
        if (error)
        {
            Msg("! Metal shader compile error: %s", error->localizedDescription()->utf8String());
            // error is autoreleased — do NOT release it manually
        }
        return false;
    }

    outFunction = library->newFunction(NS::String::string(entryPoint, NS::UTF8StringEncoding));
    library->release();

    if (!outFunction)
    {
        Msg("! Metal function '%s' not found in compiled library", entryPoint);
        return false;
    }

    return true;
}

void unregister_shader_func(u32 id)
{
    auto it = s_shaderFuncs.find(id);
    if (it != s_shaderFuncs.end())
    {
        static_cast<MTL::Function*>(it->second)->release();
        s_shaderFuncs.erase(it);
    }
}

} // namespace xray::render::RENDER_NAMESPACE
