Shader "Hidden/Klinker/Test/Filler"
{
    Properties
    {
        _MainTex("", 2D) = "" {}
    }

    CGINCLUDE

    #include "UnityCG.cginc"

    sampler2D _MainTex;
    uint _FrameCount;

    half4 Fragment(v2f_img input) : SV_Target
    {
        float2 uv = input.uv;

        half4 src = tex2D(_MainTex, uv);

        float t = _FrameCount / 60.0f;
        uint c20 = (_FrameCount + 5) % 20;
        uint c60 = (_FrameCount + 5) % 60;
    #if ENABLE_FLASH
        half fill = (c60 < 2 || c20 < 1) ^ (uv.x < frac(t));
    #else
        half fill = uv.x < frac(t);
    #endif

        return lerp(fill, src, src.a);
    }

    ENDCG

    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment
            #pragma multi_compile _ ENABLE_FLASH
            ENDCG
        }
    }
}
