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
            #pragma shader_feature _MAPRENDER_SCENEVIEW
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
            float4 _MainTex_TexelSize;
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
                
            // MAPRENDER
            float4 _LayerColor;
            int _MapRenderDepth;
            float4 _MapRenderDepthRange;
            float4 _MapRenderDepthNearColor;
            float4 _MapRenderDepthFarColor;
            int _MapRenderDepthQuantizeCount;
            float2 _MapRenderCameraZRange;
            float2 _MapRenderClip;

            // WORLD LIGHTS
            float3 _WorldLightRays[32];
            float3 _WorldLightColors[32];

            // SELECTION
            float4 _FORGE_SELECTION_COLOR;

            // COLLAPSE
            float4x4 _ManipulatorArray[32];
            float4 _ManipulatorValues[32];
            float _ManipulatorFalloffRadius[32];
            float _ManipulatorFalloffs[32];
            int _ManipulatorTypes[32];
            int _ManipulatorArrayCount;


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

                if (_ManipulatorArrayCount > 0)
                {
                    for (int i = 0; i < _ManipulatorArrayCount; ++i)
                    {
                        float3 manCenter = float3(mul(_ManipulatorArray[i], float4(0,0,0,1)).xyz);
                        float3 tpos = mul(_ManipulatorArray[i], float4(o.wpos.xyz, 1));
                        if ((tpos.x >= -0.5 && tpos.x <= 0.5) && (tpos.y >= -0.5 && tpos.y <= 0.5) && (tpos.z >= -0.5 && tpos.z <= 0.5))
                        {
                            float3 wpos = float3(o.wpos);
                            switch ((int)_ManipulatorTypes[i])
                            {
                                case 0: // shift
                                {
                                    wpos += _ManipulatorValues[i];
                                    break;
                                }
                                case 1: // collapse
                                {
                                    wpos = _ManipulatorValues[i];
                                    break;
                                }
                            }
                            
                            float dist = length(tpos);
                            float t = pow(saturate(1-dist) * _ManipulatorFalloffRadius[i], _ManipulatorFalloffs[i]);
                            o.wpos = lerp(o.wpos, wpos, t);
                            o.pos = UnityWorldToClipPos(o.wpos);
                            break;
                        }
                    }
                }

                return o;
            }

            float4 frag(v2f i, float FacingSign : VFACE) : SV_Target
            {
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
                
                float depth = saturate(i.pos.z / i.pos.w);
                if (depth < _MapRenderClip.x || depth > _MapRenderClip.y) clip(-1);
                
                if (_MapRenderDepth) {
                    float clamped = clamp(depth, _MapRenderDepthRange.x, _MapRenderDepthRange.y);
                    float d = clamped / (_MapRenderDepthRange.y - _MapRenderDepthRange.z);
                    d = pow(d, _MapRenderDepthRange.w);
                    float q = (int)(d * (_MapRenderDepthQuantizeCount)) / (float)(_MapRenderDepthQuantizeCount-1);
                    return lerp(_MapRenderDepthNearColor, _MapRenderDepthFarColor, 1-saturate(q));
                }

                return _LayerColor;
#elif _MAPRENDER_SCENEVIEW
    
                float4 col = tex2D(_MainTex, i.uv) * _LayerColor;
                clip(col.a - _AlphaClip);
                if (_RenderIgnore) clip(-1);
                
                float depth = 1-saturate((_MapRenderCameraZRange.y - i.wpos.y) / (_MapRenderCameraZRange.y - _MapRenderCameraZRange.x));
                if (depth < _MapRenderClip.x || depth > _MapRenderClip.y) clip(-1);

                if (_MapRenderDepth) {
                    float clamped = clamp(depth, _MapRenderDepthRange.x, _MapRenderDepthRange.y);
                    float d = clamped / (_MapRenderDepthRange.y - _MapRenderDepthRange.z);
                    d = pow(d, _MapRenderDepthRange.w);
                    float q = (int)(d * (_MapRenderDepthQuantizeCount)) / (float)(_MapRenderDepthQuantizeCount-1);
                    return lerp(_MapRenderDepthNearColor, _MapRenderDepthFarColor, 1-saturate(q));
                }

                return _LayerColor;
#else

                FacingSign = lerp(FacingSign, 1, _Reflection);

                // vertex colors
                float4 vcolor = lerp(1, i.color, _VertexColors);

                // sample the texture
                float4 col = tex2D(_MainTex, i.uv);
                if (col.a < 1) col.a = saturate(col.a * 2);
                
                // world lighting
                int worldLightIdx = _WorldLightIndex * 2;
                if (worldLightIdx >= 0) {

                    float d0 = saturate(dot(_WorldLightRays[worldLightIdx + 0], -FacingSign * i.normal));
                    float d1 = saturate(dot(_WorldLightRays[worldLightIdx + 1], -FacingSign * i.normal));

                    float3 c0 = d0 * _WorldLightColors[worldLightIdx + 0];
                    float3 c1 = d1 * _WorldLightColors[worldLightIdx + 1];

                    col.rgb *= (_Color.rgb * vcolor.rgb + (c0 + c1));
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
