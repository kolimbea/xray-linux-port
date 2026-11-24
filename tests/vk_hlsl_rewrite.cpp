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
        decl + "x = s_a.Load((int3(p, 0)) + int3((off), 0));");
    expect("non-literal offset, Texture2DArray", decl + "x = s_arr . Load( c , int2(i, j) );",
        decl + "x = s_arr.Load((c) + int4((int2(i, j)), 0, 0));");
    expect("literal offsets stay", decl + "a = s_a.Load(c, int2(1, -1)); b = s_a.Load(c, 3);",
        decl + "a = s_a.Load(c, int2(1, -1)); b = s_a.Load(c, 3);");
    expect("no offset stays", decl + "a = s_a.Load(c);", decl + "a = s_a.Load(c);");
    expect("other texture types stay", decl + "a = s_vol.Load(c, o);", decl + "a = s_vol.Load(c, o);");
    expect("undeclared receiver stays", "a = t.Load(c, o);", "a = t.Load(c, o);");
    expect("nested parentheses in arguments", decl + "a = s_a.Load(int3(f(x, (y)), 0), g(o[1], 2));",
        decl + "a = s_a.Load((int3(f(x, (y)), 0)) + int3((g(o[1], 2)), 0));");
    expect("macro lines are left alone", decl + "#define L(t) t.Load(c, o)\n s_a.Load(c, o);",
        decl + "#define L(t) t.Load(c, o)\n s_a.Load((c) + int3((o), 0));");
    expect("two uses in one line", decl + "s_a.Load(c, o) + s_a.Load(d, p)",
        decl + "s_a.Load((c) + int3((o), 0)) + s_a.Load((d) + int3((p), 0))");
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
    expect("both rules", decl + "[unroll(n)] x = s_a.Load(c, o);", decl + "[unroll] x = s_a.Load((c) + int3((o), 0));");

    printf(failures ? "FAILED (%d)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
