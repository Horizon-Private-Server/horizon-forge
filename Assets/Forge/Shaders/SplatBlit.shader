Shader "Horizon Forge/SplatBlit"
{
    Properties
    {
        _MainTex("Main Texture", 2D) = "white" {}
        _SplatMap("SplatMap", 2D) = "white" {}
        _Splat0("Splat0", 2D) = "white" {}
        _Splat1("Splat1", 2D) = "white" {}
        _Splat2("Splat2", 2D) = "white" {}
        _Splat3("Splat3", 2D) = "white" {}
    }

    CGINCLUDE

        #include "UnityCG.cginc"
        #include "Noise.hlsl"
        
		struct appdata
		{
			float4 vertex : POSITION;
			float2 texcoord : TEXCOORD0;
			UNITY_VERTEX_INPUT_INSTANCE_ID
		};

        struct v2f
        {
            float2 uv : TEXCOORD0;
            float4 vertex : SV_POSITION;
        };
        
        sampler2D _SplatMap;
        float4 _SplatMap_ST;
        
        sampler2D _NoiseMap;
        float4 _NoiseMap_ST;

        float2 _CurvatureCenter;

        sampler2D _Splat0;
        float4 _Splat0_ST;
        sampler2D _Splat1;
        float4 _Splat1_ST;
        sampler2D _Splat2;
        float4 _Splat2_ST;
        sampler2D _Splat3;
        float4 _Splat3_ST;

        float4 _SplatEx0;
        float4 _SplatEx1;
        float4 _SplatEx2;
        float4 _SplatEx3;

        float _SplatReduce;

        v2f VertBlit(appdata v)
        {
            v2f o;
            o.vertex = UnityObjectToClipPos(v.vertex);
            o.uv = v.texcoord;
            return o;
        }

        float4 normalize_splat(float4 col)
        {
            float mag = col.r + col.g + col.b + col.a;
            return col * (1.0 / mag);
        }

        float SampleSplatChannel(float2 uv, float4 noise, float4 ex, int channel)
        {
            float sharpness = 1 + (ex.r*10);
            float noiseFactor = saturate(1-ex.g);
            float growthFactor = ex.b * 2;
            float growth = lerp(1, ((2 - growthFactor) + noise.g*noiseFactor*4), _SplatReduce);

            float2 center = noise.r*noiseFactor*_CurvatureCenter + 0.5;
            float2 splatUV = (uv - center) * growth + center;
            float4 col = tex2D(_SplatMap, TRANSFORM_TEX(splatUV, _SplatMap));

            if (col[channel] == 0) return 0;

            return pow(col[channel], sharpness);
        }

        half4 FragBlit(v2f i) : SV_Target
        {
            float4 noise = tex2D(_NoiseMap, TRANSFORM_TEX(i.uv, _NoiseMap));
            float4 alpha = float4(_SplatEx0.a, _SplatEx1.a, _SplatEx2.a, _SplatEx3.a);

            //float4 col = tex2D(_SplatMap, TRANSFORM_TEX(splatUV, _SplatMap));
            float4 col = float4(
                SampleSplatChannel(i.uv, noise, _SplatEx0, 0),
                SampleSplatChannel(i.uv, noise, _SplatEx1, 1),
                SampleSplatChannel(i.uv, noise, _SplatEx2, 2),
                SampleSplatChannel(i.uv, noise, _SplatEx3, 3)
                );

            col.r *= 1 - saturate(dot(alpha.gba, col.gba));
            col.g *= 1 - saturate(dot(alpha.rba, col.rba));
            col.b *= 1 - saturate(dot(alpha.rga, col.rga));
            col.a *= 1 - saturate(dot(alpha.rgb, col.rgb));
            
            col = normalize_splat(col);
            //return col;

            float4 splat = tex2D(_Splat0, TRANSFORM_TEX(i.uv, _Splat0))*col.r
                 + tex2D(_Splat1, TRANSFORM_TEX(i.uv, _Splat1))*col.g
                 + tex2D(_Splat2, TRANSFORM_TEX(i.uv, _Splat2))*col.b
                 + tex2D(_Splat3, TRANSFORM_TEX(i.uv, _Splat3))*col.a;

            splat.a = 1; // terrain textures don't use alpha
            return splat;
        }

    ENDCG

    SubShader
    {
        Cull Off ZWrite Off ZTest Always

        Pass
        {
            CGPROGRAM

                #pragma vertex VertBlit
                #pragma fragment FragBlit

            ENDCG
        }
    }
}
