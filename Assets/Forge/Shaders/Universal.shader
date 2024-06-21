// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

// Upgrade NOTE: replaced '_Object2World' with 'unity_ObjectToWorld'

Shader "Horizon Forge/Universal"
{
    Properties
    {
        _Id("Id", Integer) = 0
        [Toggle] _Faded("Hidden", Integer) = 0
        [HideInInspector] _Faded2("Faded2", Integer) = 0
        [HideInInspector] _Picking("Picking", Integer) = 0
        [HideInInspector] _Selected("Selected", Integer) = 0
        [Enum(Off,0,On,1)]_ZWrite("ZWrite", Float) = 1.0
        [Enum(UnityEngine.Rendering.CompareFunction)] _ZTest("ZTest", Float) = 4 //"LessEqual"
        [Enum(UnityEngine.Rendering.CullMode)] _Culling("Culling", Float) = 0
        _Offset("Offset", Float) = 0
        [Toggle] _Transparent("Transparent", Integer) = 0
        _Smoothness("Smoothness", Range(0,1)) = 0
        _AlphaClip("AlphaClip", Range(0,1)) = 0
        _Color("Color", Color) = (1,1,1,1)
        _Rim("Rim", Range(0,1)) = 0.25
        _Shading("Shading", Range(0,1)) = 0.5
        _WorldLightIndex("World Light Index", Integer) = 0
        [Toggle] _Fog("Fog", Float) = 1.0
        [Toggle] _RenderIgnore("Render Ignore", Float) = 0.0
        [HideInInspector] _IdColor("IdColor", Color) = (0,0,0,1)
        [HideInInspector] _LayerColor("LayerColor", Color) = (0,0,0,0)
        [HideInInspector] _Reflection("Reflection", Integer) = 0
        [HideInInspector] _VertexColors("VertexColors", Integer) = 0
        _MainTex("Texture", 2D) = "white" {}
    }
    SubShader
    {
        Tags {"Queue" = "Geometry" "IgnoreProjector" = "True" "RenderType" = "Opaque"}
        LOD 100
        Lighting On
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite[_ZWrite]
        ZTest[_ZTest]
        Cull[_Culling]
        Offset[_Offset],1

        Pass
        {
            Tags {"LightMode" = "ForwardBase"}
            
            CGPROGRAM
            #pragma multi_compile_fwdbase
            #pragma vertex vert
            #pragma fragment frag
            // make fog work
            #pragma multi_compile_fog
            #pragma shader_feature _OCCLUSION
            #pragma shader_feature _MAPRENDER
            #pragma shader_feature _DEPTH
            #pragma shader_feature _PICKING

            #include "UnityCG.cginc"
            #include "AutoLight.cginc"
            #include "UniversalInc.hlsl"

            struct appdata
            {
                float4 vertex : POSITION;
                float3 normal : NORMAL;
                float2 uv : TEXCOORD0;
                float4 color : COLOR;
            };

            struct v2f
            {
                float4 color : TEXCOORD0;
                float2 uv : TEXCOORD2;
                float4 pos : SV_POSITION;
                float3 wpos : TEXCOORD3;
                float3 normal : TEXCOORD4;
            };

            sampler2D _MainTex;
            float4 _MainTex_ST;
            int _Id;
            int _Faded;
            int _Faded2;
            int _Picking;
            int _Transparent;
            int _Selected;
            int _Reflection;
            int _VertexColors;
            int _WorldLightIndex;
            float _Smoothness;
            float4 _Color;
            float _AlphaClip;
            float4 _IdColor;
            float4 _LayerColor;
            float _Rim;
            float _Shading;
            float _Fog;
            float _RenderIgnore;
            int _Mask;
            uniform float4x4 _Reflection2 = float4x4(
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1
                );
                
            float3 _WorldLightRays[32];
            float3 _WorldLightColors[32];
            float4 _FORGE_SELECTION_COLOR;
            
            float2 ClampWhole(float2 uv, float margin)
            {
                int2 uvInt = int2(uv);
	            float2 dt = uv - uvInt;

                float2 uvlow = uvInt + margin;
                float2 uvhigh = (uvInt+1) - margin;
                
                if (abs(dt.x) < margin)
                    uv.x = uvlow.x;
                else if (abs(dt.x-1) < margin)
                    uv.x = uvhigh.x;
                    
                if (abs(dt.y) < margin)
                    uv.y = uvlow.y;
                else if (abs(dt.y-1) < margin)
                    uv.y = uvhigh.y;

                return uv;
            }

            v2f vert (appdata v)
            {
                v2f o;

                float4 vo = mul(_Reflection2, mul(unity_ObjectToWorld, v.vertex));
                //float4 vn = mul(_Reflection2, mul(unity_ObjectToWorld, float4(v.vertex.xyz + v.normal.xyz, 1))) - vo;
                float4 vn = mul(_Reflection2, float4(mul(unity_ObjectToWorld, float4(v.normal, 0)).xyz, 0));
                //float4 vn = float4(mul(unity_ObjectToWorld, float4(v.normal, 0)).xyz, 0);

                //float4 vo = mul(unity_ObjectToWorld, v.vertex);
                o.pos = UnityWorldToClipPos(vo);

                //o.pos = UnityObjectToClipPos(v.vertex);
                //o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv = TRANSFORM_TEX(v.uv, _MainTex);
                o.wpos = vo.xyz;
                o.normal = normalize(vn).xyz;
                o.color = v.color;

                return o;
            }

            float4 frag(v2f i, float FacingSign : VFACE) : SV_Target
            {
                i.uv = ClampWhole(i.uv, 0.005);

#ifdef _DEPTH
                if (_RenderIgnore) clip(-1);

                float d = i.pos.z / i.pos.w;
                return float4(d,d,d,1);
#elif _PICKING
                if (_RenderIgnore) clip(-1);
                if (_Picking == 0) clip(-1);

                // alpha clip
                float4 col = tex2D(_MainTex, i.uv);
                if (col.a < 1) col.a *= 2;
                col *= _Color;
                clip(col.a - _AlphaClip);

                _IdColor.a = 1;
                return _IdColor;
#elif _OCCLUSION
                if (_RenderIgnore) clip(-1);
                _IdColor.a = 1;
                return _IdColor;
#elif _MAPRENDER
                float4 col = tex2D(_MainTex, i.uv) * _LayerColor;
                clip(col.a - _AlphaClip);
                if (_RenderIgnore) clip(-1);
                return _LayerColor;
#else

                FacingSign = lerp(FacingSign, 1, _Reflection);

                // vertex colors
                float4 vcolor = lerp(1, i.color * 2, _VertexColors);

                // sample the texture
                float4 col = tex2D(_MainTex, i.uv);
                if (col.a < 1) col.a = saturate(col.a * 2);
                
                // world lighting
                int worldLightIdx = _WorldLightIndex * 2;
                if (worldLightIdx >= 0) {

                    float d0 = (dot(_WorldLightRays[worldLightIdx + 0], -FacingSign * i.normal));
                    float d1 = (dot(_WorldLightRays[worldLightIdx + 1], -FacingSign * i.normal));

                    float3 c0 = saturate(0 + d0) * _WorldLightColors[worldLightIdx + 0];
                    float3 c1 = saturate(0 + d1) * _WorldLightColors[worldLightIdx + 1];

                    col.rgb *= (_Color.rgb * vcolor.rgb + (c0 + c1) * 0.5);
                    col.a *= _Color.a * vcolor.a;
                } else {
                    //col *= _Color * vcolor;
                    col.rgb *= (_Color.rgb * vcolor.rgb);
                    col.a *= _Color.a * vcolor.a;
                }

                // lighting
                // float3 lightDirection = normalize(_WorldSpaceLightPos0.xyz);
                // if (dot(lightDirection, lightDirection) > 0.0001) {
                //     float d = -dot(-FacingSign * i.normal, lightDirection);
                //     col.rgb *= lerp(1, (d + 1) / 2, _Shading);
                // }
                
                // rim lighting
                float3 viewDirection = normalize(_WorldSpaceCameraPos.xyz - i.wpos.xyz);
                half rim = 1.0 - (abs(dot(viewDirection, FacingSign * i.normal)) * _Rim);// rimlight based on view and normal
                col.rgb *= rim;

                if (_Faded > 0 || _Faded2 > 0)
                {
                    clip(-1);
                }
                else
                {
                    clip(col.a - _AlphaClip);
                }

                if (_Transparent == 0)
                {
                    col.a = 1;
                }

                if (_Selected > 0)
                {
                    col.rgb = lerp(col.rgb, _FORGE_SELECTION_COLOR.rgb, _FORGE_SELECTION_COLOR.a);
                }

                float fogZ = LinearEyeDepth(i.pos.z);
                float fogFactor = _Fog * saturate((fogZ - _FORGE_FOG_NEAR_DISTANCE) / (_FORGE_FOG_FAR_DISTANCE - _FORGE_FOG_NEAR_DISTANCE));
                return fixed4(lerp(col.rgb, _FORGE_FOG_COLOR.rgb, lerp(_FORGE_FOG_NEAR_INTENSITY, _FORGE_FOG_FAR_INTENSITY, fogFactor)), col.a);
#endif
            }
            ENDCG
        }
    }
}
