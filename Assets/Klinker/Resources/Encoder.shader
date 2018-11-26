Shader "Hidden/Klinker/Encoder"
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
            #include "Encoder.cginc"
            ENDCG
        }
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment Fragment_UYVY
            #pragma multi_compile _ UNITY_COLORSPACE_GAMMA
            #define ENCODER_INTERLACE
            #include "Encoder.cginc"
            ENDCG
        }
    }
}
