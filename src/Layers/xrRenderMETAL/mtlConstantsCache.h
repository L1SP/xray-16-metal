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
    static xr_vector<u8>& scratch()
    {
        static xr_vector<u8> buf;
        return buf;
    }

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

    // For Metal: location is the buffer slot ([[buffer(N)]]), NOT a uniform location.
    // Array elements share the same buffer slot; e is the element index within the array.

    ICF void seta(R_constant* C, u32 e, const Fmatrix& A)
    {
        // Use volatile barrier to prevent optimizer from removing null check
        // (compiler sees &*array UB in SkeletonX.cpp and proves C non-null)
        R_constant* volatile Cp = C;
        if (!Cp) return;
        R_constant_load L;
        if (C->destination & RC_dest_pixel)   L = C->ps;
        if (C->destination & RC_dest_vertex)  L = C->vs;
        if (C->destination & RC_dest_geometry) L = C->gs;
        if (C->destination & RC_dest_all)     L = C->pp;
        append_at_offset(C, L, &A, sizeof(Fmatrix), e * sizeof(Fmatrix));
    }

    ICF void seta(R_constant* C, u32 e, const Fvector4& A)
    {
        R_constant* volatile Cp = C;
        if (!Cp) return;
        R_constant_load L;
        if (C->destination & RC_dest_pixel)   L = C->ps;
        if (C->destination & RC_dest_vertex)  L = C->vs;
        if (C->destination & RC_dest_geometry) L = C->gs;
        if (C->destination & RC_dest_all)     L = C->pp;
        append_at_offset(C, L, &A, sizeof(Fvector4), e * sizeof(Fvector4));
    }

    ICF void seta(R_constant* C, u32 e, float x, float y, float z, float w)
    {
        R_constant* volatile Cp = C;
        if (!Cp) return;
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

        // Collect unique locations (linear scan — small list, max ~256)
        u32 locs[MaxPending];
        u32 numLocs = 0;
        for (size_t i = 0; i < pending().size(); i++)
        {
            u32 loc = pending()[i].location;
            bool found = false;
            for (u32 j = 0; j < numLocs; j++)
                if (locs[j] == loc) { found = true; break; }
            if (!found)
                locs[numLocs++] = loc;
        }

        auto& sc = scratch();

        for (u32 li = 0; li < numLocs; li++)
        {
            u32 loc = locs[li];

            // Find max byte extent for this location
            u32 maxEnd = 0;
            u32 destFlags = 0;
            for (size_t i = 0; i < pending().size(); i++)
            {
                const auto& p = pending()[i];
                if (p.location != loc) continue;
                destFlags = p.dest;
                u32 end = (p.offset == u32(-1)) ? p.size : (p.offset + p.size);
                if (end > maxEnd) maxEnd = end;
            }
            if (maxEnd == 0) continue;

            // Copy all entries for this location into a scratch buffer
            sc.assign(maxEnd, 0);
            for (size_t i = 0; i < pending().size(); i++)
            {
                const auto& p = pending()[i];
                if (p.location != loc) continue;
                u32 dstOff = (p.offset == u32(-1)) ? 0 : p.offset;
                memcpy(&sc[dstOff], p.data, p.size);
            }

            // Upload to encoder
            if (destFlags & RC_dest_vertex)
                enc->setVertexBytes(sc.data(), maxEnd, loc);
            if (destFlags & RC_dest_pixel)
                enc->setFragmentBytes(sc.data(), maxEnd, loc);
            if (destFlags & RC_dest_all)
            {
                enc->setVertexBytes(sc.data(), maxEnd, loc);
                enc->setFragmentBytes(sc.data(), maxEnd, loc);
            }
        }

        pending().clear();
    }

    ICF void reset()
    {
        pending().clear();
    }
};
} // namespace xray::render::RENDER_NAMESPACE
