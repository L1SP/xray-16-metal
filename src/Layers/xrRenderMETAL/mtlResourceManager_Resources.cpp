#include "stdafx.h"
#pragma hdrstop

#include "../xrRender/ResourceManager.h"
#include "../xrRender/tss.h"
#include "../xrRender/Blender.h"
#include "../xrRender/Blender_Recorder.h"
#include "Layers/xrRender/BufferUtils.h"
#include "Layers/xrRender/ShaderResourceTraits.h"

namespace xray::render::RENDER_NAMESPACE
{
static u32 s_declUID = 1;

static u32 get_null_ps_id()
{
    static u32 id = 0;
    if (id != 0)
        return id;

    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (!device)
        return 0;

    NS::String* src = NS::String::string("fragment float4 null_main() { return float4(0); }", NS::UTF8StringEncoding);
    NS::Error* err = nullptr;
    auto* lib = device->newLibrary(src, nullptr, &err);
    if (!lib)
    {
        if (err)
            Msg("! null PS library creation failed: %s", err->localizedDescription()->utf8String());
        return 0;
    }

    auto* func = lib->newFunction(NS::String::string("null_main", NS::UTF8StringEncoding));
    lib->release();
    if (!func)
    {
        Msg("! null PS function not found in library");
        return 0;
    }

    void* ptr = func;
    register_shader_func(ptr, id);
    return id;
}

SPass* CResourceManager::_CreatePass(const SPass& proto)
{
    for (SPass* pass : v_passes)
        if (pass->equal(proto))
            return pass;

    SPass* P = v_passes.emplace_back(xr_new<SPass>());
    P->dwFlags |= xr_resource_flagged::RF_REGISTERED;
    P->state = proto.state;
    P->ps = proto.ps;
    P->vs = proto.vs;
    P->gs = proto.gs;
    P->constants = proto.constants;
    P->T = proto.T;
#ifdef _EDITOR
    P->M = proto.M;
#endif
    P->C = proto.C;

    return P;
}

SDeclaration* CResourceManager::_CreateDecl(const D3DVERTEXELEMENT9* dcl)
{
    for (SDeclaration* D : v_declarations)
    {
        if (!D->dcl_code.empty() && dcl_equal(dcl, &D->dcl_code.front()))
            return D;
    }

    SDeclaration* D = v_declarations.emplace_back(xr_new<SDeclaration>());
    D->dcl = ++s_declUID; // unique declaration ID for PSO cache key

    u32 dcl_size = GetDeclLength(dcl) + 1;
    D->dcl_code.assign(dcl, dcl + dcl_size);
    ConvertVertexDeclaration(dcl, D);
    D->dwFlags |= xr_resource_flagged::RF_REGISTERED;

    return D;
}

SVS* CResourceManager::_CreateVS(cpcstr shader, u32 flags)
{
    string_path name;
    xr_strcpy(name, shader);
    switch (RImplementation.m_skinning)
    {
    case 0: xr_strcat(name, "_0"); break;
    case 1: xr_strcat(name, "_1"); break;
    case 2: xr_strcat(name, "_2"); break;
    case 3: xr_strcat(name, "_3"); break;
    case 4: xr_strcat(name, "_4"); break;
    // m_skinning < 0 → no suffix → SKIN_NONE
    }
    auto* result = CreateShader<SVS>(name, shader, flags);
    if (result && result->sh == 0)
    {
        // CreateShader short-circuited for name=="null" (m_skinning < 0, no suffix).
        // Metal requires a real function; force compilation by using a proxy cache key
        // that won't match the "null" string.
        string_path proxy;
        xr_strcpy(proxy, name);
        xr_strcat(proxy, "$");
        auto* proxyResult = CreateShader<SVS>(proxy, shader, flags);
        if (proxyResult && proxyResult->sh != 0)
            result->sh = proxyResult->sh;
    }
    return result;
}

void CResourceManager::_DeleteVS(const SVS* vs) { DestroyShader(vs); }

SPS* CResourceManager::_CreatePS(LPCSTR _name)
{
    string_path name;
    xr_strcpy(name, _name);
    switch (RImplementation.m_MSAASample)
    {
    case 0: xr_strcat(name, "_0"); break;
    case 1: xr_strcat(name, "_1"); break;
    case 2: xr_strcat(name, "_2"); break;
    case 3: xr_strcat(name, "_3"); break;
    case 4: xr_strcat(name, "_4"); break;
    case 5: xr_strcat(name, "_5"); break;
    case 6: xr_strcat(name, "_6"); break;
    case 7: xr_strcat(name, "_7"); break;
    }
    SPS* result = CreateShader<SPS>(name, _name);
    if (result && result->sh == 0)
    {
        u32 id = get_null_ps_id();
        if (id)
            result->sh = id;
    }
    return result;
}

void CResourceManager::_DeletePS(const SPS* ps) { DestroyShader(ps); }

SGS* CResourceManager::_CreateGS(LPCSTR Name) { return CreateShader<SGS>(Name); }
void CResourceManager::_DeleteGS(const SGS* gs) { DestroyShader(gs); }

SHS* CResourceManager::_CreateHS(LPCSTR Name) { return CreateShader<SHS>(Name); }
void CResourceManager::_DeleteHS(const SHS* HS) { DestroyShader(HS); }

SDS* CResourceManager::_CreateDS(LPCSTR Name) { return CreateShader<SDS>(Name); }
void CResourceManager::_DeleteDS(const SDS* DS) { DestroyShader(DS); }

SCS* CResourceManager::_CreateCS(LPCSTR Name) { return CreateShader<SCS>(Name); }
void CResourceManager::_DeleteCS(const SCS* CS) { DestroyShader(CS); }
} // namespace xray::render::RENDER_NAMESPACE
