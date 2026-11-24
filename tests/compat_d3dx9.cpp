// Tests for monolith/src/Common/linux/d3dx9_linux.cpp (expected values derived by hand from the
#include "../monolith/src/xrCore/xrCore_platform.h"
// D3DX9 documentation formulas). Build command: see tests/README.md.
#include <d3d9types.h>
#include <d3dx9.h>
#include <cmath>
#include <cstdio>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main()
{
    D3DXMATRIX t, s, m, inv, id;
    D3DXMatrixTranslation(&t, 1, 2, 3);
    D3DXMatrixScaling(&s, 2, 4, 8);
    D3DXMatrixMultiply(&m, &s, &t); // scale then translate (row vectors)
    D3DXVECTOR3 p(1, 1, 1), q;
    D3DXVec3TransformCoordArray(&q, sizeof(q), &p, sizeof(p), &m, 1);
    CHECK(near(q.x, 3) && near(q.y, 6) && near(q.z, 11));
    float det = 0;
    CHECK(D3DXMatrixInverse(&inv, &det, &m) == &inv);
    CHECK(near(det, 64));
    D3DXMatrixMultiply(&id, &m, &inv);
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) CHECK(near(id.m[i][j], i == j ? 1.f : 0.f));
    D3DXMATRIX zero; memset(&zero, 0, sizeof(zero));
    CHECK(D3DXMatrixInverse(&inv, &det, &zero) == nullptr);
    // aliasing: out == m1
    D3DXMATRIX a = s; D3DXMatrixMultiply(&a, &a, &t);
    CHECK(!memcmp(&a, &m, sizeof(m)));
    // ortho: maps (l,b,zn) -> (-1,-1,0) and (r,t,zf) -> (1,1,1)
    D3DXMATRIX o; D3DXMatrixOrthoOffCenterLH(&o, 0, 800, 600, 0, 0, 1);
    D3DXVECTOR3 c0(0, 600, 0), c1(800, 0, 1), r0, r1;
    D3DXVec3TransformCoordArray(&r0, sizeof(r0), &c0, sizeof(c0), &o, 1);
    D3DXVec3TransformCoordArray(&r1, sizeof(r1), &c1, sizeof(c1), &o, 1);
    CHECK(near(r0.x, -1) && near(r0.y, -1) && near(r0.z, 0) && near(r1.x, 1) && near(r1.y, 1) && near(r1.z, 1));
    D3DXVECTOR3 n(3, 0, 4), nn; D3DXVec3Normalize(&nn, &n); CHECK(near(nn.x, .6f) && near(nn.z, .8f));
    D3DXVECTOR3 z0(0, 0, 0); D3DXVec3Normalize(&nn, &z0); CHECK(nn.x == 0 && nn.y == 0 && nn.z == 0);
    D3DXVec3TransformNormal(&nn, &p, &m); CHECK(near(nn.x, 2) && near(nn.y, 4) && near(nn.z, 8));
    // half floats
    const float in[6] = {1.f, -2.f, 0.f, 65504.f, 1e-8f, 0.333333f};
    D3DXFLOAT16 h[6]; D3DXFloat32To16Array(h, in, 6);
    const WORD want[6] = {0x3c00, 0xc000, 0x0000, 0x7bff, 0x0000, 0x3555};
    for (int i = 0; i < 6; ++i) { WORD w; memcpy(&w, &h[i], 2); CHECK(w == want[i]); }
    // FVF -> declaration -> sizes agree with D3DXGetFVFVertexSize
    const DWORD fvfs[] = {D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX3,
        D3DFVF_XYZB3 | D3DFVF_LASTBETA_UBYTE4 | D3DFVF_NORMAL | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE3(1)};
    for (DWORD f : fvfs)
    {
        D3DVERTEXELEMENT9 d[MAX_FVF_DECL_SIZE];
        CHECK(D3DXDeclaratorFromFVF(f, d) == D3D_OK);
        CHECK(D3DXGetDeclVertexSize(d, 0) == D3DXGetFVFVertexSize(f));
    }
    D3DVERTEXELEMENT9 d[MAX_FVF_DECL_SIZE];
    D3DXDeclaratorFromFVF(fvfs[2], d);
    CHECK(D3DXGetDeclLength(d) == 6); // pos, weights(FLOAT2), indices, normal, tex0, tex1
    CHECK(d[1].Type == D3DDECLTYPE_FLOAT2 && d[1].Usage == D3DDECLUSAGE_BLENDWEIGHT);
    CHECK(d[2].Type == D3DDECLTYPE_UBYTE4 && d[2].Usage == D3DDECLUSAGE_BLENDINDICES);
    CHECK(d[5].Type == D3DDECLTYPE_FLOAT3 && d[5].UsageIndex == 1);
    printf(failures ? "FAILED (%d)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
