Shader "Horizon Forge/Collider"
{
    Properties
    {
        [HideInInspector] _ColId("Collision Id", Integer) = 0
        [HideInInspector] _Faded2("Faded2", Integer) = 0
        [HideInInspector] _Picking("Picking", Integer) = 0
        [HideInInspector] _Selected("Selected", Integer) = 0
    }
    SubShader
    {
        Tags {"Queue" = "Geometry" "IgnoreProjector" = "True" "RenderType" = "Opaque"}
        LOD 100
        Lighting On
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite On
        ZTest LEqual
        Cull Back
		Offset -1,1

        Pass
        {
            Tags {"LightMode" = "ForwardBase"}
            
            CGPROGRAM
            #pragma multi_compile_fwdbase
			#pragma vertex vert
			#pragma geometry geom
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

            int _ColId;
            int _Faded2;
            int _Picking;
            int _Selected;
            float4 _IdColor;
            uniform float4x4 _Reflection2 = float4x4(
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1
                );
                
			struct appdata
			{
				float4 vertex : POSITION;
				float2 texcoord0 : TEXCOORD0;
				float4 normal : NORMAL;
				UNITY_VERTEX_INPUT_INSTANCE_ID
			};

			struct v2g
			{
				float4 projectionSpaceVertex : SV_POSITION;
				float2 uv0 : TEXCOORD0;
				float4 worldSpacePosition : TEXCOORD1;
				float4 worldSpaceNormal : TEXCOORD2;
				UNITY_VERTEX_OUTPUT_STEREO
			};

			struct g2f
			{
				float4 projectionSpaceVertex : SV_POSITION;
				float2 uv0 : TEXCOORD0;
				float4 worldSpacePosition : TEXCOORD1;
				float4 dist : TEXCOORD2;
				float4 worldSpaceNormal : TEXCOORD3;
				UNITY_VERTEX_OUTPUT_STEREO
			};
			
			v2g vert (appdata v)
			{
				v2g o;
				UNITY_SETUP_INSTANCE_ID(v);
				UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);
				o.projectionSpaceVertex = UnityObjectToClipPos(v.vertex);
				o.worldSpacePosition = mul(unity_ObjectToWorld, v.vertex);
				o.uv0 = v.texcoord0;
				o.worldSpaceNormal = mul(unity_ObjectToWorld, float4(v.normal.xyz, 0));
				return o;
			}
			
			[maxvertexcount(3)]
			void geom(triangle v2g i[3], inout TriangleStream<g2f> triangleStream)
			{
				float _WireThickness = 0.1;

				float2 p0 = i[0].projectionSpaceVertex.xy / i[0].projectionSpaceVertex.w;
				float2 p1 = i[1].projectionSpaceVertex.xy / i[1].projectionSpaceVertex.w;
				float2 p2 = i[2].projectionSpaceVertex.xy / i[2].projectionSpaceVertex.w;

				float2 edge0 = p2 - p1;
				float2 edge1 = p2 - p0;
				float2 edge2 = p1 - p0;

				// To find the distance to the opposite edge, we take the
				// formula for finding the area of a triangle Area = Base/2 * Height, 
				// and solve for the Height = (Area * 2)/Base.
				// We can get the area of a triangle by taking its cross product
				// divided by 2.  However we can avoid dividing our area/base by 2
				// since our cross product will already be double our area.
				float area = abs(edge1.x * edge2.y - edge1.y * edge2.x);
				float wireThickness = 800 - _WireThickness;

				g2f o;
				
				o.uv0 = i[0].uv0;
				o.worldSpacePosition = i[0].worldSpacePosition;
				o.projectionSpaceVertex = i[0].projectionSpaceVertex;
				o.dist.xyz = float3( (area / length(edge0)), 0.0, 0.0) * o.projectionSpaceVertex.w * wireThickness;
				o.dist.w = 1.0 / o.projectionSpaceVertex.w;
				o.worldSpaceNormal = i[0].worldSpaceNormal;
				UNITY_TRANSFER_VERTEX_OUTPUT_STEREO(i[0], o);
				triangleStream.Append(o);

				o.uv0 = i[1].uv0;
				o.worldSpacePosition = i[1].worldSpacePosition;
				o.projectionSpaceVertex = i[1].projectionSpaceVertex;
				o.dist.xyz = float3(0.0, (area / length(edge1)), 0.0) * o.projectionSpaceVertex.w * wireThickness;
				o.dist.w = 1.0 / o.projectionSpaceVertex.w;
				o.worldSpaceNormal = i[1].worldSpaceNormal;
				UNITY_TRANSFER_VERTEX_OUTPUT_STEREO(i[1], o);
				triangleStream.Append(o);

				o.uv0 = i[2].uv0;
				o.worldSpacePosition = i[2].worldSpacePosition;
				o.projectionSpaceVertex = i[2].projectionSpaceVertex;
				o.dist.xyz = float3(0.0, 0.0, (area / length(edge2))) * o.projectionSpaceVertex.w * wireThickness;
				o.dist.w = 1.0 / o.projectionSpaceVertex.w;
				o.worldSpaceNormal = i[2].worldSpaceNormal;
				UNITY_TRANSFER_VERTEX_OUTPUT_STEREO(i[2], o);
				triangleStream.Append(o);
			}

			fixed4 frag (g2f i) : SV_Target
			{
#ifdef _DEPTH
                clip(-1);
                return 0;
#elif _PICKING
                if (_Picking == 0) clip(-1);
                _IdColor.a = 1;
                return _IdColor;
#elif _OCCLUSION
                clip(-1);
                return 0;
#elif _MAPRENDER
                clip(-1);
                return 0;
#endif

				float _WireSmoothness = 1;
				float4 _WireColor = 1;
				float minDistanceToEdge = min(i.dist[0], min(i.dist[1], i.dist[2])) * i.dist[3];

				float4 baseColor = 1;
				baseColor.r = ((_ColId & 0x03) << 6) / 255.0;
                baseColor.g = ((_ColId & 0x0C) << 4)  / 255.0;
                baseColor.b = ((_ColId & 0xF0) << 0)  / 255.0;
				
				// rim lighting
                float3 viewDirection = normalize(_WorldSpaceCameraPos.xyz - i.worldSpacePosition.xyz);
                half rim = 1.0 - (abs(dot(viewDirection, normalize(i.worldSpaceNormal))) * 0.25);// rimlight based on view and normal
                baseColor.rgb *= rim;

				// Early out if we know we are not on a line segment.
				if(minDistanceToEdge > 0.9)
				{
					//clip(-1);
					return fixed4(baseColor.rgb,1);
				}

				// Smooth our line out
				float t = exp2(_WireSmoothness * -1.0 * minDistanceToEdge * minDistanceToEdge);
				fixed4 finalColor = lerp(baseColor, _WireColor, t);
				finalColor.a = t;

				return finalColor;
			}
            ENDCG
        }
    }
}
