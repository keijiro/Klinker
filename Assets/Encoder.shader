Shader "Hidden/Klinker/Encoder"
{
    Properties
    {
        _MainTex("", 2D) = "" {}
    }

    CGINCLUDE

    #include "UnityCG.cginc"

    sampler2D _MainTex;
    float4 _MainTex_TexelSize;

    // Adobe-esque HDTV Rec.709 (2.2 gamma, 16-235 limit)
    half3 RGB2YUV(half3 rgb)
    {
        const half K_B = 0.0722;
        const half K_R = 0.2126;

    #if !UNITY_COLORSPACE_GAMMA
        rgb = LinearToGammaSpace(rgb);
    #endif

        half y = dot(half3(K_R, 1 - K_B - K_R, K_B), rgb);
        half u = ((rgb.b - y) / (1 - K_B) * 112 + 128) / 255;
        half v = ((rgb.r - y) / (1 - K_R) * 112 + 128) / 255;

        y = (y * 219 + 16) / 255;

        return half3(y, u, v);
    }

    half4 Fragment_UYVY(v2f_img input) : SV_Target
    {
        const float3 ts = float3(_MainTex_TexelSize.xy, 0);

        float2 uv = input.uv;
        uv.x -= ts.x * 0.5;
        uv.y = 1 - uv.y;

        half3 yuv1 = RGB2YUV(tex2D(_MainTex, uv        ).rgb);
        half3 yuv2 = RGB2YUV(tex2D(_MainTex, uv + ts.xz).rgb);

        half u = (yuv1.y + yuv2.y) * 0.5;
        half v = (yuv1.z + yuv2.z) * 0.5;

        return half4(u, yuv1.x, v, yuv2.x);
    }

    ENDCG

    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment_UYVY
            #pragma multi_compile _ UNITY_COLORSPACE_GAMMA
            ENDCG
        }
    }
}
