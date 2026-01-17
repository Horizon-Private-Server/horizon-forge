Shader "Horizon Forge/TintBlit"
{
    Properties {
        _In("_In", 2D) = "white" {}
        _Color("_Color", Color) = (1,1,1,1)
        _ForceAlpha("_ForceAlpha", Float) = 0
    }

    SubShader {
        Tags { "RenderType"="Opaque" }
        Pass {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            sampler2D _In;
            float4 _Color;
            float _ForceAlpha;

            struct appdata {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct v2f {
                float2 uv : TEXCOORD0;
                float4 pos : SV_POSITION;
            };

            v2f vert(appdata v) {
                v2f o;
                o.pos = UnityObjectToClipPos(v.vertex);
                o.uv = v.uv;
                return o;
            }

            fixed4 frag(v2f i) : SV_Target {
                float4 tex = tex2D(_In, i.uv);
                float4 outColor = tex * _Color;
                
                // Optionally force alpha to 1.0
                if (_ForceAlpha > 0)
                    outColor.a = _ForceAlpha;

                // force proper rounding before writing to 8-bit target
                outColor.a = round(outColor.a * 255.0) / 255.0;

                return outColor;
            }
            ENDCG
        }
    }
}
