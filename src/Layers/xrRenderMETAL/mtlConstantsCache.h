#pragma once

namespace xray::render::RENDER_NAMESPACE
{
class ECORE_API R_constants
{
    enum { MaxPending = 256 };

    struct Pending
    {
        u32 location;
        u32 dest;
        u32 size;
        u32 data[16]; // max 64 bytes
    };

    static xr_vector<Pending>& pending()
    {
        static xr_vector<Pending> buf;
        return buf;
    }

    ICF void append(R_constant* C, R_constant_load& L, const void* data, u32 size)
    {
        auto& buf = pending();
        if (buf.size() < MaxPending)
        {
            Pending p;
            p.location = L.location;
            p.dest = C->destination;
            p.size = size;
            memcpy(p.data, data, size);
            buf.push_back(p);
        }
    }

private:
    ICF void set(R_constant* C, R_constant_load& L, const Fmatrix& A)
    {
        VERIFY(RC_float == C->type);
        append(C, L, &A, sizeof(Fmatrix));
    }

    ICF void set(R_constant* C, R_constant_load& L, const Fvector4& A)
    {
        VERIFY(RC_float == C->type);
        append(C, L, &A, sizeof(Fvector4));
    }

    ICF void set(R_constant* C, R_constant_load& L, float x, float y, float z, float w)
    {
        VERIFY(RC_float == C->type);
        float v[4] = { x, y, z, w };
        append(C, L, v, sizeof(v));
    }

    ICF void set(R_constant* C, R_constant_load& L, float A)
    {
        VERIFY(RC_float == C->type);
        append(C, L, &A, sizeof(float));
    }

    ICF void set(R_constant* C, R_constant_load& L, int A)
    {
        VERIFY(RC_int == C->type);
        append(C, L, &A, sizeof(int));
    }

public:
    ICF void set(R_constant* C, const Fmatrix& A)
    {
        if (C->destination & RC_dest_pixel)   set(C, C->ps, A);
        if (C->destination & RC_dest_vertex)  set(C, C->vs, A);
        if (C->destination & RC_dest_geometry) set(C, C->gs, A);
        if (C->destination & RC_dest_all)     set(C, C->pp, A);
    }

    ICF void set(R_constant* C, const Fvector4& A)
    {
        if (C->destination & RC_dest_pixel)   set(C, C->ps, A);
        if (C->destination & RC_dest_vertex)  set(C, C->vs, A);
        if (C->destination & RC_dest_geometry) set(C, C->gs, A);
        if (C->destination & RC_dest_all)     set(C, C->pp, A);
    }

    ICF void set(R_constant* C, float x, float y, float z, float w)
    {
        if (C->destination & RC_dest_pixel)   set(C, C->ps, x, y, z, w);
        if (C->destination & RC_dest_vertex)  set(C, C->vs, x, y, z, w);
        if (C->destination & RC_dest_geometry) set(C, C->gs, x, y, z, w);
        if (C->destination & RC_dest_all)     set(C, C->pp, x, y, z, w);
    }

    ICF void set(R_constant* C, float A)
    {
        if (C->destination & RC_dest_pixel)   set(C, C->ps, A);
        if (C->destination & RC_dest_vertex)  set(C, C->vs, A);
        if (C->destination & RC_dest_geometry) set(C, C->gs, A);
        if (C->destination & RC_dest_all)     set(C, C->pp, A);
    }

    ICF void set(R_constant* C, int A)
    {
        if (C->destination & RC_dest_pixel)   set(C, C->ps, A);
        if (C->destination & RC_dest_vertex)  set(C, C->vs, A);
        if (C->destination & RC_dest_geometry) set(C, C->gs, A);
        if (C->destination & RC_dest_all)     set(C, C->pp, A);
    }

    ICF void seta(R_constant* C, u32 e, const Fmatrix& A)
    {
        R_constant_load L;
        if (C->destination & RC_dest_pixel)   L = C->ps;
        if (C->destination & RC_dest_vertex)  L = C->vs;
        if (C->destination & RC_dest_geometry) L = C->gs;
        if (C->destination & RC_dest_all)     L = C->pp;
        L.location += e;
        set(C, L, A);
    }

    ICF void seta(R_constant* C, u32 e, const Fvector4& A)
    {
        R_constant_load L;
        if (C->destination & RC_dest_pixel)   L = C->ps;
        if (C->destination & RC_dest_vertex)  L = C->vs;
        if (C->destination & RC_dest_geometry) L = C->gs;
        if (C->destination & RC_dest_all)     L = C->pp;
        L.location += e;
        set(C, L, A);
    }

    ICF void seta(R_constant* C, u32 e, float x, float y, float z, float w)
    {
        R_constant_load L;
        if (C->destination & RC_dest_pixel)   L = C->ps;
        if (C->destination & RC_dest_vertex)  L = C->vs;
        if (C->destination & RC_dest_geometry) L = C->gs;
        if (C->destination & RC_dest_all)     L = C->pp;
        L.location += e;
        set(C, L, x, y, z, w);
    }

    ICF void flush()
    {
        auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
        if (!enc)
        {
            pending().clear();
            return;
        }
        if (pending().empty())
            return;

        for (size_t i = 0; i < pending().size(); i++)
        {
            const auto& p = pending()[i];
            if (p.dest & RC_dest_vertex)
                enc->setVertexBytes(p.data, p.size, p.location);
            if (p.dest & RC_dest_pixel)
                enc->setFragmentBytes(p.data, p.size, p.location);
            if (p.dest & RC_dest_geometry)
                enc->setVertexBytes(p.data, p.size, p.location);
            if (p.dest & RC_dest_all)
            {
                enc->setVertexBytes(p.data, p.size, p.location);
                enc->setFragmentBytes(p.data, p.size, p.location);
            }
        }
    }

    ICF void reset()
    {
        pending().clear();
    }
};
} // namespace xray::render::RENDER_NAMESPACE
