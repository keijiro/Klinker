// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

#include "UnityCG.cginc"

sampler2D _MainTex;
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

half4 Fragment(v2f_img input) : SV_Target
{
    float2 uv = input.uv;
    float4 ts = _MainTex_TexelSize;

#if UNITY_UV_STARTS_AT_TOP
    uv.y = 1 - uv.y;
#endif

    // Deinterlacing
    //
    // * Sampling pattern for odd field
    //
    //     |   |
    // 5.0 +   +
    //     | x | 4.5 : Sample point for 4.0 - 6.0
    // 4.0 +---+
    //     |   |
    // 3.0 +   +
    //     | x | 2.5 : Sample point for 2.0 - 4.0
    // 2.0 +---+
    //     |   |
    // 1.0 +   +
    //     | x | 0.5 : Sample point for 0.0 - 2.0
    // 0.0 +---+
    //
    // * Sampling pattern for even field
    //
    //     | x | 5.5 : Sample point for 5.0 - 7.0
    // 5.0 +---+
    //     |   |
    // 4.0 +   +
    //     | x | 3.5 : Sample point for 3.0 - 5.0
    // 3.0 +---+
    //     |   |
    // 2.0 +   +
    //     | x | 1.5 : Sample point for 1.0 - 3.0
    // 1.0 +---+
    //     |   |
    // 0.0 +---+

#if defined(KLINKER_DEINTERLACE_ODD)
    uv.y = (floor((uv.y * ts.w + 0) / 2) * 2 + 0.5) * ts.y;
#elif defined(KLINKER_DEINTERLACE_EVEN)
    uv.y = (floor((uv.y * ts.w + 1) / 2) * 2 - 0.5) * ts.y;
#endif

    // Upsample from 4:2:2
    half4 uyvy = tex2D(_MainTex, uv);
    bool sel = frac(uv.x * ts.z) < 0.5;
    half3 yuv = sel ? uyvy.yxz : uyvy.wxz;

    return half4(YUV2RGB(yuv), 1);
}
