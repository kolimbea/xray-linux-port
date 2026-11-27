// Unit test of the HLSL compatibility rules (monolith/src/Layers/xrRenderVK/vkHlslRewrite.cpp).
#include "vkHlslRewrite.h"

#include <cstdio>
#include <string>

static int failures = 0;

static void expect(const char *what, const std::string &in, const std::string &want)
{
    const std::string got = xrvk::hlsl::rewrite_compat(in);
    if (got != want)
    {
        printf("FAIL %s\n  in:   %s\n  want: %s\n  got:  %s\n", what, in.c_str(), want.c_str(), got.c_str());
        ++failures;
    }
}

int main()
{
    const std::string decl = "Texture2D<float4> s_a;\nTexture2DArray s_arr;\nTexture3D s_vol;\n";

    // rule 1
    expect("non-literal offset, Texture2D", decl + "x = s_a.Load(int3(p, 0), off);",
        decl + "x = s_a.Load((int3(p, 0)) + int3((int2)(off), 0));");
    expect("non-literal offset, Texture2DArray", decl + "x = s_arr . Load( c , int2(i, j) );",
        decl + "x = s_arr.Load((c) + int4((int2)(int2(i, j)), 0, 0));");
    expect("literal offsets stay", decl + "a = s_a.Load(c, int2(1, -1)); b = s_a.Load(c, 3);",
        decl + "a = s_a.Load(c, int2(1, -1)); b = s_a.Load(c, 3);");
    expect("no offset stays", decl + "a = s_a.Load(c);", decl + "a = s_a.Load(c);");
    expect("other texture types stay", decl + "a = s_vol.Load(c, o);", decl + "a = s_vol.Load(c, o);");
    expect("undeclared receiver stays", "a = t.Load(c, o);", "a = t.Load(c, o);");
    expect("nested parentheses in arguments", decl + "a = s_a.Load(int3(f(x, (y)), 0), g(o[1], 2));",
        decl + "a = s_a.Load((int3(f(x, (y)), 0)) + int3((int2)(g(o[1], 2)), 0));");
    expect("macro lines are left alone", decl + "#define L(t) t.Load(c, o)\n s_a.Load(c, o);",
        decl + "#define L(t) t.Load(c, o)\n s_a.Load((c) + int3((int2)(o), 0));");
    expect("two uses in one line", decl + "s_a.Load(c, o) + s_a.Load(d, p)",
        decl + "s_a.Load((c) + int3((int2)(o), 0)) + s_a.Load((d) + int3((int2)(p), 0))");
    expect("Load as part of a longer name stays", decl + "s_a.LoadX(c, o); s_a.MyLoad(c, o);",
        decl + "s_a.LoadX(c, o); s_a.MyLoad(c, o);");
    expect("unbalanced call stays", decl + "s_a.Load(c, o", decl + "s_a.Load(c, o");

    // rule 2
    expect("unroll with expression", "[unroll(N * 2)] for (;;) {}", "[unroll] for (;;) {}");
    expect("loop with array index", "[loop( counts[q] )]for(;;){}", "[loop]for(;;){}");
    expect("literal counts stay", "[unroll(4)] [loop( 8 )]", "[unroll(4)] [loop( 8 )]");
    expect("plain attributes stay", "[unroll] [loop] [branch]", "[unroll] [loop] [branch]");
    expect("array subscripts stay", "a[unrollx(1)] b[i]", "a[unrollx(1)] b[i]");

    // edge cases
    expect("empty", "", "");
    expect("both rules", decl + "[unroll(n)] x = s_a.Load(c, o);", decl + "[unroll] x = s_a.Load((c) + int3((int2)(o), 0));");

    // rule 3: drop an unused parameter whose input semantic duplicates another one
    {
        const std::string pre = "#line 1 \"hlsl.hlsl\"\n#line 1 \"./inc.h\"\nstruct p_flat { float2 tc : TEXCOORD0; };\n#line 2 \"hlsl.hlsl\"\n"
                                "float4 main (p_flat I, float4 pos2d: SV_Position, float2 tc0: TEXCOORD0): SV_Target\n{ return pos2d; }\n";
        // DXC reports the column of the semantic: line 2 of hlsl.hlsl
        const size_t col = std::string("float4 main (p_flat I, float4 pos2d: SV_Position, float2 tc0: ").size() + 1;
        const std::string diag = "hlsl.hlsl:2:" + std::to_string(col) + ": error: input semantic 'TEXCOORD0' used more than once\n";
        std::string text = pre;
        const bool changed = xrvk::hlsl::drop_unused_duplicate_input(text, diag);
        const std::string want = "#line 1 \"hlsl.hlsl\"\n#line 1 \"./inc.h\"\nstruct p_flat { float2 tc : TEXCOORD0; };\n#line 2 \"hlsl.hlsl\"\n"
                                 "float4 main (p_flat I, float4 pos2d: SV_Position): SV_Target\n{ return pos2d; }\n";
        if (!changed || text != want)
        {
            printf("FAIL rule 3 (unused duplicate)\n  got: %s\n", text.c_str());
            ++failures;
        }
        // the body reads it: unchanged
        std::string used = pre;
        used.replace(used.find("return pos2d;"), 13, "return tc0.xyxy;");
        const std::string before = used;
        if (xrvk::hlsl::drop_unused_duplicate_input(used, diag) || used != before)
        {
            printf("FAIL rule 3 must keep a used parameter\n");
            ++failures;
        }
        // other diagnostics: unchanged
        std::string other = pre;
        if (xrvk::hlsl::drop_unused_duplicate_input(other, "hlsl.hlsl:2:5: error: something else\n"))
        {
            printf("FAIL rule 3 must ignore other errors\n");
            ++failures;
        }
    }

    printf(failures ? "FAILED (%d)\n" : "all checks passed\n", failures);
    // rule 4
    expect("lowercase texture2D declaration", "uniform texture2D jitter4;\nx = jitter4.Load(int3(p, 0));",
        "uniform Texture2D jitter4;\nx = jitter4.Load(int3(p, 0));");
    expect("texture3D and textureCUBE", "texture3D v; textureCUBE c; textureCube d;",
        "Texture3D v; TextureCube c; TextureCube d;");
    expect("rule 1 applies to a lowercase declaration", "texture2D t;\nx = t.Load(c, o);",
        "Texture2D t;\nx = t.Load((c) + int3((int2)(o), 0));");
    expect("longer names and directives stay", "my_texture2D a; texture2Dx b;\n#define texture2D foo\n",
        "my_texture2D a; texture2Dx b;\n#define texture2D foo\n");
    expect("scalar offset in parentheses is widened", decl + "x = s_a.Load(int3(p, 0), ( 0 ));",
        decl + "x = s_a.Load((int3(p, 0)) + int3((int2)(( 0 )), 0));");
    // rule 5
    expect("pow2 partitioning", "[domain(\"tri\")]\n[partitioning( \"pow2\" )]\n[outputtopology(\"triangle_cw\")]",
        "[domain(\"tri\")]\n[partitioning( \"integer\" )]\n[outputtopology(\"triangle_cw\")]");
    expect("other partitioning stays", "[partitioning(\"fractional_odd\")]", "[partitioning(\"fractional_odd\")]");
    return failures ? 1 : 0;
}
