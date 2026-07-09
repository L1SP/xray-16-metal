#pragma once

namespace xray::render::RENDER_NAMESPACE
{
class ECORE_API R_constants
{
    enum { MaxPending = 512 };

    struct Pending
    {
        u32 location;
        u32 dest;
        u32 size;
        u32 offset; // offset in bytes (u32(-1) = no offset = standalone constant)
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
            p.offset = u32(-1);
            memcpy(p.data, data, size);
            buf.push_back(p);
        }
    }

    ICF void append_at_offset(R_constant* C, R_constant_load& L, const void* data, u32 size, u32 offset)
    {
        auto& buf = pending();
        if (buf.size() < MaxPending)
        {
            Pending p;
            p.location = L.location;
            p.dest = C->destination;
            p.size = size;
            p.offset = offset;
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
        if (L.cls == RC_3x4)
        {
            float data[12];
            data[0] = A._11; data[1] = A._21; data[2] = A._31;
            data[3] = A._12; data[4] = A._22; data[5] = A._32;
            data[6] = A._13; data[7] = A._23; data[8] = A._33;
            data[9] = A._41; data[10] = A._42; data[11] = A._43;
            append_at_offset(C, L, data, 48, e * 48);
        }
        else
        {
            append_at_offset(C, L, &A, sizeof(Fmatrix), e * sizeof(Fmatrix));
        }
    }

    ICF void seta(R_constant* C, u32 e, const Fvector4& A)
    {
        R_constant_load L;
        if (C->destination & RC_dest_pixel)   L = C->ps;
        if (C->destination & RC_dest_vertex)  L = C->vs;
        if (C->destination & RC_dest_geometry) L = C->gs;
        if (C->destination & RC_dest_all)     L = C->pp;
        append_at_offset(C, L, &A, sizeof(Fvector4), e * sizeof(Fvector4));
    }

    ICF void seta(R_constant* C, u32 e, float x, float y, float z, float w)
    {
        R_constant_load L;
        if (C->destination & RC_dest_pixel)   L = C->ps;
        if (C->destination & RC_dest_vertex)  L = C->vs;
        if (C->destination & RC_dest_geometry) L = C->gs;
        if (C->destination & RC_dest_all)     L = C->pp;
        float v[4] = { x, y, z, w };
        append_at_offset(C, L, v, sizeof(v), e * sizeof(v));
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
            if (p.offset != u32(-1))
                continue;

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

        u32 locs[16];
        u32 numLocs = 0;
        for (auto& p : pending())
        {
            if (p.offset == u32(-1))
                continue;
            bool found = false;
            for (u32 j = 0; j < numLocs; j++)
                if (locs[j] == p.location)
                {
                    found = true;
                    break;
                }
            if (!found && numLocs < 16)
                locs[numLocs++] = p.location;
        }

        for (u32 li = 0; li < numLocs; li++)
        {
            u32 loc = locs[li];
            u32 totalSize = 0;
            for (auto& p : pending())
            {
                if (p.offset == u32(-1) || p.location != loc)
                    continue;
                u32 end = p.offset + p.size;
                if (end > totalSize)
                    totalSize = end;
            }
            if (totalSize == 0)
                continue;

            xr_vector<u8> buf(totalSize, 0);
            u32 dest = RC_dest_vertex;
            for (auto& p : pending())
            {
                if (p.offset == u32(-1) || p.location != loc)
                    continue;
                memcpy(buf.data() + p.offset, p.data, p.size);
                dest = p.dest;
            }

            if (dest & RC_dest_vertex)
                enc->setVertexBytes(buf.data(), totalSize, loc);
            if (dest & RC_dest_pixel)
                enc->setFragmentBytes(buf.data(), totalSize, loc);
            if (dest & RC_dest_geometry)
                enc->setVertexBytes(buf.data(), totalSize, loc);
            if (dest & RC_dest_all)
            {
                enc->setVertexBytes(buf.data(), totalSize, loc);
                enc->setFragmentBytes(buf.data(), totalSize, loc);
            }
        }
    }

    ICF void reset()
    {
        pending().clear();
    }
};
} // namespace xray::render::RENDER_NAMESPACE
