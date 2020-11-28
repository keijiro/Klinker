// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

Shader "Hidden/Klinker/Subsampler"
{
    Properties
    {
        _MainTex("", 2D) = "" {}
        _FieldTex("", 2D) = "" {}
    }
    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment_UYVY
            #pragma multi_compile _ UNITY_COLORSPACE_GAMMA
            #include "Subsampler.cginc"
            ENDCG
        }
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment_UYVY
            #pragma multi_compile _ UNITY_COLORSPACE_GAMMA
            #define SUBSAMPLER_INTERLACE
            #include "Subsampler.cginc"
            ENDCG
        }
    }
}
