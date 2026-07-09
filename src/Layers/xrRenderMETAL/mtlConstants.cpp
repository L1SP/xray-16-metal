#include "stdafx.h"
#pragma hdrstop

#include "../xrRender/r_constants.h"

namespace xray::render::RENDER_NAMESPACE
{
static class cl_sampler : public R_constant_setup
{
    void setup(CBackend& cmd_list, R_constant* C) override
    {
    }
} binder_sampler;

// Map GLSL uniform type strings to RC_* element class
static u32 glsl_type_to_cls(pcstr& pos)
{
    // Skip whitespace
    while (*pos == ' ' || *pos == '\t')
        ++pos;

    if (strncmp(pos, "float4x4", 8) == 0) { pos += 8; return RC_4x4; }
    if (strncmp(pos, "float3x4", 8) == 0) { pos += 8; return RC_3x4; }
    if (strncmp(pos, "float2x4", 8) == 0) { pos += 8; return RC_2x4; }
    if (strncmp(pos, "float4", 6) == 0)   { pos += 6; return RC_1x4; }
    if (strncmp(pos, "float3", 6) == 0)   { pos += 6; return RC_1x3; }
    if (strncmp(pos, "float2", 6) == 0)   { pos += 6; return RC_1x2; }
    if (strncmp(pos, "float", 5) == 0)    { pos += 5; return RC_1x1; }
    if (strncmp(pos, "int", 3) == 0)      { pos += 3; return RC_1x1; }
    if (strncmp(pos, "sampler2D", 9) == 0){ pos += 9; return RC_sampler; }
    if (strncmp(pos, "sampler3D", 9) == 0){ pos += 9; return RC_sampler; }
    if (strncmp(pos, "samplerCube", 11) == 0) { pos += 11; return RC_sampler; }
    if (strncmp(pos, "sampler2DMS", 11) == 0) { pos += 11; return RC_sampler; }
    if (strncmp(pos, "sampler2DShadow", 15) == 0) { pos += 15; return RC_sampler; }
    return u32(-1);
}

// New: parse the MSL source to find constant parameters in main0()
// Only ACTIVE uniforms (present in the compiled MSL) are added to the table.
// This returns a map of name → (buffer_index, type_class).
struct MslUniformInfo { u32 bufferIndex; u32 typeCls; };
using MslUniformMap = xr_map<xr_string, MslUniformInfo>;

static MslUniformMap parse_msl_uniforms(pcstr mslSource)
{
    MslUniformMap result;
    if (!mslSource || !mslSource[0])
        return result;

    // Find main0( ... ) and parse its parameters
    const char* p = strstr(mslSource, "main0(");
    if (!p) return result;
    p += 6; // skip "main0("

    // Find matching closing paren
    int depth = 1;
    const char* end = p;
    while (*end && depth > 0)
    {
        if (*end == '(') depth++;
        else if (*end == ')') depth--;
        if (depth > 0) end++;
    }
    if (depth != 0) return result;

        // Extract parameter list
    xr_string params(p, end - p);

    // Parse each parameter: look for "constant <type>& <name> [[buffer(N)]]"
    size_t pos = 0;
    while (pos < params.length())
    {
        // Find "constant " keyword
        size_t cpos = params.find("constant ", pos);
        if (cpos == xr_string::npos) break;

        // After "constant ", find the type (skip type till we hit &)
        size_t tstart = cpos + 9;
        // Skip spaces
        while (tstart < params.length() && (params[tstart] == ' ' || params[tstart] == '\t'))
            tstart++;

        // Find the '&' which separates type from name
        size_t amp = params.find('&', tstart);
        if (amp == xr_string::npos) { pos = cpos + 9; continue; }

        // Extract type string
        xr_string typeStr = params.substr(tstart, amp - tstart);
        // Trim trailing spaces
        while (!typeStr.empty() && (typeStr.back() == ' ' || typeStr.back() == '\t'))
            typeStr.pop_back();

        // Extract name (after &, before [, space, or end)
        size_t nstart = amp + 1;
        while (nstart < params.length() && (params[nstart] == ' ' || params[nstart] == '&'))
            nstart++;
        size_t nend = nstart;
        while (nend < params.length() && params[nend] != ' ' && params[nend] != ',' && params[nend] != ')' && params[nend] != '[')
            nend++;
        xr_string name = params.substr(nstart, nend - nstart);
        if (name.empty()) { pos = cpos + 9; continue; }

        // Find [[buffer(N)]]
        size_t buffer_kw_pos = params.find("[[buffer(", nend);
        u32 bufferIdx = 0;
        if (buffer_kw_pos != xr_string::npos)
        {
            const char* num_start = params.c_str() + buffer_kw_pos + 9;
            char* endp = nullptr;
            bufferIdx = (u32)strtoul(num_start, &endp, 10);
        }

        // Map type string to class
        u32 cls = RC_1x1;
        if (typeStr == "float4x4") cls = RC_4x4;
        else if (typeStr == "float4x3") cls = RC_3x4; // this line cost me 2 days of my life
        else if (typeStr == "float3x4") cls = RC_3x4;
        else if (typeStr == "float2x4") cls = RC_2x4;
        else if (typeStr == "float4" || typeStr == "half4") cls = RC_1x4;
        else if (typeStr == "float3" || typeStr == "half3") cls = RC_1x3;
        else if (typeStr == "float2" || typeStr == "half2") cls = RC_1x2;
        else if (typeStr == "float" || typeStr == "half") cls = RC_1x1;
        else if (typeStr.find("spvUnsafeArray<float4") != xr_string::npos) cls = RC_4x4;
        else if (typeStr.find("spvUnsafeArray<float2") != xr_string::npos) cls = RC_2x4;
        else if (typeStr.find("spvUnsafeArray<int") != xr_string::npos) cls = RC_int;

        // SPIRV-Cross renames GLSL array uniforms (like `float4x4 sbones_array[128]`)
        // to `spvUnsafeArray<float4, N>& array [[buffer(N)]]` in MSL (older versions)
        // or preserves the name as `sbones_array` (newer versions).
        // Map to RC_3x4 — the engine treats sbones_array as an array of 3×4 matrices
        // (3 consecutive float4s per bone, 48-byte stride). SPIRV-Cross emits
        // spvUnsafeArray<float4> which parse_msl_uniforms types as RC_4x4, but seta()
        // needs RC_3x4 to select the correct 48-byte stride and row-major float4 packing.
        xr_string mappedName = name;
        if ((name == "array" || name == "sbones_array") && typeStr.find("spvUnsafeArray") != xr_string::npos)
        {
            mappedName = "sbones_array";
            cls = RC_3x4;
        }

        result[mappedName] = { bufferIdx, cls };
        pos = nend;
    }
    return result;
}

// Old-style parse from GLSL source: only updates entries that already exist
// and were created by parse_msl_uniforms. This is kept for compatibility
// but the primary population happens from MSL now.
static void parse_glsl_uniforms(R_constant_table& table, pcstr source, u32 destination)
{
    if (!source)
        return;

    pcstr p = source;
    while (*p)
    {
        // Look for 'uniform' keyword
        if (strncmp(p, "uniform", 7) == 0)
        {
            pcstr decl = p + 7;
            u32 cls = glsl_type_to_cls(decl);
            if (cls != u32(-1))
            {
                // Skip whitespace
                while (*decl == ' ' || *decl == '\t')
                    ++decl;

                // Extract name (up to ;, whitespace, or [ for arrays)
                pcstr nameStart = decl;
                while (*decl && *decl != ';' && *decl != ' ' && *decl != '\t' && *decl != '\n' && *decl != '\r' && *decl != '[' && *decl != ':')
                    ++decl;
                if (decl > nameStart)
                {
                    // Build name string
                    xr_string name(nameStart, decl - nameStart);
                    ref_constant C = table.get(name.c_str());
                    if (C)
                    {
                        if (cls == RC_sampler)
                        {
                            // Already handled by pre-seeding; set type if not already
                            C->type = RC_sampler;
                        }
                        else
                        {
                            // Set element class - location will be set from MSL parse
                            if (destination & RC_dest_vertex)
                                C->vs.cls = cls;
                            if (destination & RC_dest_pixel)
                                C->ps.cls = cls;
                            if (destination & RC_dest_geometry)
                                C->gs.cls = cls;
                            if (destination & RC_dest_all)
                                C->pp.cls = cls;
                        }
                    }
                }
            }
            // Advance past this uniform declaration
            while (*p && *p != ';')
                ++p;
            if (*p == ';')
                ++p;
            continue;
        }
        ++p;
    }
}

// Populate constant table from MSL source (active uniforms only)
void setup_constants_from_msl(R_constant_table& table, pcstr mslSource, u32 destination)
{
    auto mslUniforms = parse_msl_uniforms(mslSource);
    for (auto& [name, info] : mslUniforms)
    {
        ref_constant C = table.get(name.c_str());
        if (!C)
        {
            C = table.table.emplace_back(xr_new<R_constant>());
            C->name = name.c_str();
            C->destination = destination;
            C->type = RC_float;
        }
        else
        {
            C->destination |= destination;
        }

        R_constant_load& L = C->get_load(destination);
        L.cls = info.typeCls;
        L.location = info.bufferIndex; // matches MSL [[buffer(N)]]
        L.index = 0;
    }

    // Re-sort the table before texture/sampler lookups, because the buffer loop above
    // may have appended new entries (via emplace_back), breaking the sort order required
    // by table.get(name.c_str()) which uses binary search (lower_bound).
    if (table.table.size() > 1)
    {
        std::sort(table.table.begin(), table.table.end(),
            [](const ref_constant& a, const ref_constant& b)
            { return xr_strcmp(a->name.c_str(), b->name.c_str()) < 0; });
    }

    // If sbones_array wasn't found in MSL (compiled with SKIN_NONE, SPIRV-Cross omits
    // it as unused), pre-seed it with a default location so SkeletonX::_Render's
    // get_c("sbones_array") doesn't return null. The data uploaded to buffer 30 is
    // harmless — the shader doesn't sample from it when SKIN_NONE is active.
    if (!table.get("sbones_array"))
    {
        ref_constant C = table.table.emplace_back(xr_new<R_constant>());
        C->name = "sbones_array";
        C->destination = RC_dest_vertex;
        C->type = RC_float;
        R_constant_load& L = C->vs;
        L.cls = RC_3x4;
        L.location = 30;
        L.index = 0;
        // Re-sort to maintain binary search order
        std::sort(table.table.begin(), table.table.end(),
            [](const ref_constant& a, const ref_constant& b)
            { return xr_strcmp(a->name.c_str(), b->name.c_str()) < 0; });
    }

    // Parse texture/sampler bindings from MSL: look for "texture2d<float> <name> [[texture(N)]]"
    // and "sampler <name> [[sampler(N)]]" in the main0() parameter list.
    // Update the pre-seeded sampler constants with the actual MSL texture/sampler index.
    const char* p = strstr(mslSource, "main0(");
    if (!p) { Msg("* MSL texture parse: no main0() found"); return; }
    p += 6;
    int depth = 1;
    const char* end = p;
    while (*end && depth > 0)
    {
        if (*end == '(') depth++;
        else if (*end == ')') depth--;
        if (depth > 0) end++;
    }
    if (depth != 0) { Msg("* MSL texture parse: unmatched parens"); return; }

    xr_string params(p, end - p);
    // Find all texture parameters: "texture2d<float> <name> [[texture(N)]]"
    // and sampler parameters: "sampler <name> [[sampler(N)]]"
    size_t pos = 0;
    while (pos < params.length())
    {
        // Look for "texture2d" (NOT just "texture" to avoid matching [[texture(N)]] attributes)
        // and "sampler" keywords (not followed by '(' to avoid [[sampler(N)]])
        size_t texPos = params.find("texture2d", pos);
        // For sampler: find "sampler" not followed by '(' (to avoid [[sampler(N)]])
        size_t sampPos = xr_string::npos;
        {
            size_t sp = params.find("sampler", pos);
            while (sp != xr_string::npos)
            {
                // Check if this "sampler" is inside [[sampler(N)]] or is a type keyword
                size_t next = sp + 7;
                // Skip past type qualifiers like "sampler2D" or similar
                char ch = next < params.length() ? params[next] : 0;
                if (ch != '(' && ch != ')' && ch != ',' && ch != ' ' && ch != '[' && ch != ']')
                {
                    // This is part of a longer word like "sampler2D" - skip it
                    sp = params.find("sampler", next);
                    continue;
                }
                sampPos = sp;
                break;
            }
        }
        if (texPos == xr_string::npos && sampPos == xr_string::npos)
            break;
        bool isTexture = (texPos != xr_string::npos && (sampPos == xr_string::npos || texPos < sampPos));
        size_t start = isTexture ? texPos : sampPos;

        // Find the name start (skip the type keyword)
        size_t afterType = start;
        while (afterType < params.length() && params[afterType] != ' ' && params[afterType] != '>')
            afterType++;
        if (afterType >= params.length()) { pos = start + 1; continue; }
        // For texture2d<float>, skip until after '>'
        if (params[afterType] == '>')
        {
            while (afterType < params.length() && params[afterType] != ' ')
                afterType++;
        }
        while (afterType < params.length() && params[afterType] == ' ')
            afterType++;

        // Extract name
        size_t nstart = afterType;
        size_t nend = nstart;
        while (nend < params.length() && params[nend] != ' ' && params[nend] != ',' && params[nend] != ')' && params[nend] != '[')
            nend++;
        xr_string name = params.substr(nstart, nend - nstart);
        if (name.empty()) { pos = start + 1; continue; }

        // Find [[texture(N)]] or [[sampler(N)]]
        xr_string bracket = isTexture ? "[[texture(" : "[[sampler(";
        size_t bpos = params.find(bracket, nend);
        u32 bindingIdx = 0;
        if (bpos != xr_string::npos)
        {
            bpos += bracket.length();
            char* endp = nullptr;
            bindingIdx = (u32)strtoul(params.c_str() + bpos, &endp, 10);
        }

        // Find the sampler constant and update its location
        ref_constant C = table.get(name.c_str());
        if (C && C->type == RC_sampler)
        {
            // Update the sampler location to match MSL texture/sampler index
            C->samp.location = bindingIdx;
            C->samp.index = bindingIdx;
            // Also update ps location if this is a pixel shader texture
            if (destination & RC_dest_pixel)
            {
                C->ps.location = bindingIdx;
                C->ps.index = bindingIdx;
            }
        }
        else if (!C)
        {
            // Create a new sampler constant
            C = table.table.emplace_back(xr_new<R_constant>());
            C->name = name.c_str();
            C->destination = RC_dest_sampler;
            C->type = RC_sampler;
            C->handler = &binder_sampler;
            R_constant_load& L = C->samp;
            L.location = bindingIdx;
            L.index = bindingIdx;
            L.cls = RC_sampler;
            L.program = 0;
        }
        else
        {
        }

        pos = nend;
    }
}

BOOL R_constant_table::parse(void* _desc, u32 destination)
{
    auto source = static_cast<pcstr>(_desc);

    // Sort before GLSL uniform parsing — parse_glsl_uniforms uses table.get()
    // which relies on binary search (lower_bound).
    if (table.size() > 1)
    {
        std::sort(table.begin(), table.end(), [](const ref_constant& C1, const ref_constant& C2)
        {
            return xr_strcmp(C1->name, C2->name) < 0;
        });
    }

    // Parse actual GLSL source for uniform declarations (may be nullptr for stub shaders)
    if (source && *source)
        parse_glsl_uniforms(*this, source, destination);

    return TRUE;
}
} // namespace xray::render::RENDER_NAMESPACE
