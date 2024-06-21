Shader "Horizon Forge/Skymesh"
{
    Properties
    {
        _MainTex("Texture", 2D) = "white" {}
        [HDR] _Color("Color", Color) = (1,1,1,1)
        _Opacity("Opacity", Float) = 1
        [Toggle] _Bloom("Bloom", Float) = 0
    }
    SubShader
    {
        Tags { "RenderType" = "Opaque" "Queue" = "Background" }
        Cull Off
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite Off
        ZTest LEqual

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma shader_feature _OCCLUSION
            #pragma shader_feature _MAPRENDER
            #pragma shader_feature _DEPTH
            #pragma shader_feature _PICKING

            #include "UnityCG.cginc"
            #include "UnityShaderVariables.cginc"

            struct appdata
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
                float4 color : COLOR;
            };

            struct v2f
            {
                float2 uv : TEXCOORD0;
                float4 vertex : SV_POSITION;
                float4 color : COLOR;
            };

            sampler2D _MainTex;
            float4 _MainTex_ST;
            float4 _Color;
            float _Opacity;
            float _Bloom;

            v2f vert(appdata v)
            {
                v2f o;
                float4x4 a = unity_MatrixV;
                a[0].w = 0;
                a[1].w = 0;
                a[2].w = 0;
                a[3].w = 1;
                a = mul(UNITY_MATRIX_P, a);
                a = mul(a, unity_ObjectToWorld);
                o.vertex = mul(a, v.vertex);
                //o.vertex = mul(unity_MatrixMVP, v.vertex);
                o.uv = TRANSFORM_TEX(v.uv, _MainTex);
                o.color = v.color;
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
#ifdef _DEPTH
                return 0;
#elif _PICKING
                return fixed4(0,0,0,1);
#elif _OCCLUSION
                return 0;
#elif _MAPRENDER
                return 0;
#else
                // sample the texture
                fixed4 col = tex2D(_MainTex, i.uv);

                float bloom = _Bloom * 0.5;

                float a = saturate(i.color.a * col.a * _Color.a * _Opacity);
                //col.a = a; //*a;

                //col.rgb *= _Color.rgb * (1 + bloom);
                //col.rgb = float3(a, 1, 1);

                fixed4 final = col;
                final.rgb *= _Color.rgb * (1 + bloom);
                final.a = a;
                return final;
#endif
            }
            ENDCG
        }
    }
}
