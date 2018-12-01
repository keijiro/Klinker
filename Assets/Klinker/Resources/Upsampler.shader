// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

Shader "Hidden/Klinker/Upsampler"
{
    Properties
    {
        _MainTex("", 2D) = "" {}
    }
    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment
            #pragma multi_compile _ UNITY_COLORSPACE_GAMMA
            #include "Upsampler.cginc"
            ENDCG
        }
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment
            #pragma multi_compile _ UNITY_COLORSPACE_GAMMA
            #define KLINKER_DEINTERLACE_ODD
            #include "Upsampler.cginc"
            ENDCG
        }
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment
            #pragma multi_compile _ UNITY_COLORSPACE_GAMMA
            #define KLINKER_DEINTERLACE_EVEN
            #include "Upsampler.cginc"
            ENDCG
        }
    }
}
