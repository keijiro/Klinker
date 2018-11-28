#include "UnityCG.cginc"

sampler2D _MainTex;
sampler2D _FieldTex;

float4 _MainTex_TexelSize;

// Adobe-flavored HDTV Rec.709 (2.2 gamma, 16-235 limit)
half3 YUV2RGB(half3 yuv)
{
    const half K_B = 0.0722;
    const half K_R = 0.2126;

    half y = (yuv.x - 16.0 / 255) * 255 / 219;
    half u = (yuv.y - 128.0 / 255) * 255 / 112;
    half v = (yuv.z - 128.0 / 255) * 255 / 112;

    half r = y + v * (1 - K_R);
    half g = y - v * K_R / (1 - K_R) - u * K_B / (1 - K_B);
    half b = y + u * (1 - K_B);

#if UNITY_COLORSPACE_GAMMA
    return half3(r, g, b);
#else
    return GammaToLinearSpace(half3(r, g, b));
#endif
}

half4 Fragment_UYVY(v2f_img input) : SV_Target
{
    float2 uv = float2(input.uv.x, 1 - input.uv.y);

    half4 uyvy = tex2D(_MainTex, uv);
    bool sel = frac(uv.x * _MainTex_TexelSize.z) < 0.5;
    half3 yuv = sel ? uyvy.yxz : uyvy.wxz;

    return half4(YUV2RGB(yuv), 1);
}
