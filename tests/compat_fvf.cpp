#include <windows_base.h>
#include <d3d9types.h>
#include <cstdio>
#include "fvf.inc"
int main(){ struct { DWORD f; UINT n; } c[] = {
 {D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1, 32}, {D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1, 28},
 {D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_SPECULAR, 20}, {D3DFVF_XYZB4|D3DFVF_NORMAL|D3DFVF_TEX2|D3DFVF_TEXCOORDSIZE3(1), 28+12+8+12},
 {D3DFVF_XYZW|D3DFVF_PSIZE, 20} };
 int f=0; for (auto& t: c){ UINT n=xr_fvf_vertex_size(t.f); if(n!=t.n){printf("FAIL fvf=%x got %u want %u\n",t.f,n,t.n); f++;} }
 printf("fvf fails=%d\n",f); return f; }
