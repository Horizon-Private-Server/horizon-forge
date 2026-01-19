using GLTFast.Export;
using Newtonsoft.Json;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;

public static class MapExporter
{
    public static readonly float DZO_SKY_BRIGHTNESS_FACTOR = 1.5f;

    [MenuItem("Forge/Tools/Exporter/Export for DZO")]
    public static async void ExportSceneForDZO()
    {
        var path = EditorUtility.SaveFilePanel(
            "Save scene as glb",
            "",
            SceneManager.GetActiveScene().name + ".glb",
            "glb");

        if (!string.IsNullOrEmpty(path))
        {
            await ExportSceneForDZO(path, null);
        }
    }

    public static async Task<bool> ExportSceneForDZO(string outFilePath, string outMetadataFilePath)
    {
        const string objectPathPrefix = "Scene/";
        var renameHistory = new Dictionary<GameObject, string>();
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var gameObjectsToCleanup = new List<GameObject>();
        if (!mapConfig) return false;

        var exportSettings = new ExportSettings
        {
            Format = GltfFormat.Binary,
            ImageDestination = ImageDestination.MainBuffer,
            FileConflictResolution = FileConflictResolution.Overwrite,
            ComponentMask = GLTFast.ComponentType.Mesh | GLTFast.ComponentType.Light,
        };

        var gameObjectExportSettings = new GameObjectExportSettings
        {
            OnlyActiveInHierarchy = true,
            DisabledComponents = false,
            LayerMask = ~LayerMask.GetMask("COLLISION"),
        };

        var export = new GameObjectExport(exportSettings, gameObjectExportSettings);

        try
        {
            // run generators
            UnityHelper.RunGeneratorsPreBake(BakeType.BUILD);

            // we want to export static geometry
            var dzoConfig = GameObject.FindObjectOfType<DzoConfig>() ?? new DzoConfig();
            var ties = dzoConfig.Ties ? GameObject.FindObjectsOfType<Tie>() : new Tie[0];
            var shrubs = dzoConfig.Shrubs ? GameObject.FindObjectsOfType<Shrub>() : new Shrub[0];
            var tfrags = dzoConfig.Tfrags ? GameObject.FindObjectsOfType<TfragChunk>() : new TfragChunk[0];
            var lights = dzoConfig.Lights ? GameObject.FindObjectsOfType<Light>() : new Light[0];
            var sky = dzoConfig.Sky ? GameObject.FindObjectOfType<Sky>() : null;
            var convertToShrubs = dzoConfig.Shrubs ? GameObject.FindObjectsOfType<ConvertToShrub>() : new ConvertToShrub[0];
            var extraGeometry = dzoConfig.IncludeInExport ?? new GameObject[0];

            // create minimap object
            GameObject minimapGo = null;
            var minimapTex = dzoConfig.MinimapTextureOverride ? dzoConfig.MinimapTextureOverride : mapConfig.DLMinimap;
            if (minimapTex)
            {
                minimapGo = new GameObject("minimap-container");
                var mf = minimapGo.AddComponent<MeshFilter>();
                var mr = minimapGo.AddComponent<MeshRenderer>();
                var mat = new Material(Shader.Find("Standard"));
                mat.mainTexture = UnityHelper.ResizeTexture(minimapTex, dzoConfig.MinimapResolution, dzoConfig.MinimapResolution);
                mr.sharedMaterial = mat;
                mf.sharedMesh = UnityHelper.BuildQuad();

                gameObjectsToCleanup.Add(minimapGo);
            }

            var fogT = mapConfig.FogFarIntensity - mapConfig.FogNearIntensity;
            var fogRange = (mapConfig.FogFarDistance - mapConfig.FogNearDistance) / (mapConfig.FogFarIntensity - mapConfig.FogNearIntensity);
            var fogNear = (mapConfig.FogNearDistance * (1 - fogT)) - (fogRange * mapConfig.FogNearIntensity);
            var fogFar = fogNear + fogRange;
            var metadata = new DzoMapMetadata()
            {
                SkymeshName = sky ? objectPathPrefix + sky.gameObject.name : null,
                MinimapMeshName = minimapGo ? objectPathPrefix + minimapGo.name : null,
                BackgroundColor = dzoConfig.UseBackgroundColorOverride ? dzoConfig.BackgroundColorOverride : mapConfig.BackgroundColor,
                FogColor = dzoConfig.FogOverride ? dzoConfig.FogOverrideColor : mapConfig.FogColor,
                FogNearDistance = dzoConfig.FogOverride ? dzoConfig.FogOverrideNearDistance : fogNear,
                FogFarDistance = dzoConfig.FogOverride ? dzoConfig.FogOverrideFarDistance : fogFar,
                PostColorFilter = dzoConfig.PostColorFilter,
                PostExposure = dzoConfig.PostExposure,
                DefaultCameraPosition = dzoConfig.DefaultCameraPosition ? dzoConfig.DefaultCameraPosition.position : Vector3.zero,
                DefaultCameraEuler = dzoConfig.DefaultCameraPosition ? dzoConfig.DefaultCameraPosition.eulerAngles : Vector3.zero,
            };

            // merge static geometry into one object
            var staticGameObjects = ties.Select(x => x.GetAssetInstance()).Union(shrubs.Select(x => x.GetAssetInstance())).Union(tfrags.Select(x => x.gameObject)).Union(extraGeometry).Where(x => x).Distinct().ToArray();
            var combinedCopy = CombineMeshes(staticGameObjects, dzoConfig, metadata);
            if (combinedCopy) gameObjectsToCleanup.Add(combinedCopy);

            RenameIndexedUnique("light", lights, renameHistory);
            RenameIndexedUnique("sky", sky, renameHistory);

            // export scene
            var gameObjectsToExport = new List<GameObject>();
            if (combinedCopy) gameObjectsToExport.Add(combinedCopy);
            //gameObjectsToExport.AddRange(extraGeometry);
            if (minimapGo) gameObjectsToExport.Add(minimapGo);
            gameObjectsToExport.AddRange(convertToShrubs.Where(x => x.DZOExportWithShrubs).Select(x => x.gameObject));
            foreach (var light in lights) gameObjectsToExport.Add(light.gameObject);

            if (sky)
            {
                gameObjectsToExport.Add(sky.gameObject);
                foreach (var layer in sky.Layers)
                {
                    var renderer = layer.GetRenderer();
                    if (renderer)
                        renderer.enabled = true;
                }    
            }

            export.AddScene(gameObjectsToExport.ToArray());

            // save glb file
            using (var fs = File.Create(outFilePath))
                if (!await export.SaveToStreamAndDispose(fs))
                    return false;

            // save metadata
            if (!string.IsNullOrEmpty(outMetadataFilePath))
            {
                var shellData = new List<DzoMapMetadata.SkymeshShellMetadata>();
                var lightData = new List<DzoMapMetadata.LightMetadata>(); 

                if (sky)
                {
                    var shells = sky.GetComponentsInChildren<SkyLayer>();
                    if (shells != null)
                    {
                        foreach (var shell in shells)
                        {
                            var color = shell.GetMaterials()[0].GetColor("_Color").linear.ScaleRGB(DZO_SKY_BRIGHTNESS_FACTOR);
                            color.a *= shell.GetMaterials()[0].GetFloat("_Opacity");

                            shellData.Add(new DzoMapMetadata.SkymeshShellMetadata()
                            {
                                ShellName = shell.gameObject.name,
                                AngularVelocity = shell.AngularRotation,
                                Bloom = shell.Bloom,
                                Disabled = !shell.isActiveAndEnabled,
                                Color = color
                            });
                        }
                    }
                }

                foreach (var light in lights)
                {
                    lightData.Add(new DzoMapMetadata.LightMetadata()
                    {
                        LightName = objectPathPrefix + light.gameObject.name,
                        CastShadows = light.shadows != LightShadows.None,
                        ShadowStrength = light.shadowStrength
                    });
                }

                metadata.TieShrubTfragCombinedName = combinedCopy ? objectPathPrefix + combinedCopy.name : null;
                metadata.SkymeshShells = shellData.ToArray();
                metadata.Lights = lightData.ToArray();
                if (metadata.Meshes != null)
                    metadata.Meshes.ForEach(x => x.Name = metadata.TieShrubTfragCombinedName + "/" + x.Name);

                File.WriteAllText(outMetadataFilePath, JsonUtility.ToJson(metadata, true));
            }

            return true;
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
            return false;
        }
        finally
        {
            // cleanup generators
            UnityHelper.RunGeneratorsPostBake(BakeType.BUILD);

            // 
            if (gameObjectsToCleanup.Any())
                foreach (var go in gameObjectsToCleanup)
                    GameObject.DestroyImmediate(go);

            // return objects to their old names
            foreach (var rename in renameHistory)
                if (rename.Key)
                    rename.Key.name = rename.Value;
        }
    }

    static void RenameIndexedUnique<T>(string prefix, T[] objects, Dictionary<GameObject, string> renameHistory) where T : Component
    {
        if (objects == null) return;

        for (int i = 0; i < objects.Length; ++i)
        {
            var obj = objects[i].gameObject;
            renameHistory.Add(obj.gameObject, obj.name);
            obj.name = prefix + i.ToString();
        }
    }

    static void RenameIndexedUnique<T>(string prefix, T obj, Dictionary<GameObject, string> renameHistory) where T : Component
    {
        if (obj == null) return;

        renameHistory.Add(obj.gameObject, obj.name);
        obj.name = prefix;
    }

    static GameObject CombineMeshes(GameObject[] gameObjects, DzoConfig dzoConfig, DzoMapMetadata metadata)
    {
        var collisionLayer = LayerMask.NameToLayer("COLLISION");
        var universalShader = Shader.Find("Horizon Forge/Universal");

        // Locals
        Dictionary<Material, List<MeshFilterSubMesh>> materialToMeshFilterList = new Dictionary<Material, List<MeshFilterSubMesh>>();
        List<GameObject> combinedObjects = new List<GameObject>();

        MeshFilter[] meshFilters = gameObjects.SelectMany(x => x.GetComponentsInChildren<MeshFilter>()).Where(x => x && x.gameObject.layer != collisionLayer).ToArray();

        // Go through all mesh filters and establish the mapping between the materials and all mesh filters using it.
        foreach (var meshFilter in meshFilters)
        {
            // empty mesh
            if (!meshFilter || !meshFilter.sharedMesh || meshFilter.sharedMesh.vertexCount <= 0)
            {
                continue;
            }

            var meshRenderer = meshFilter.GetComponent<MeshRenderer>();
            if (meshRenderer == null)
            {
                Debug.LogWarning("Mesh Combine Wizard: The Mesh Filter on object " + meshFilter.name + " has no Mesh Renderer component attached. Skipping.");
                continue;
            }

            var materials = meshRenderer.sharedMaterials;
            if (materials == null)
            {
                Debug.LogWarning("Mesh Combine Wizard: The Mesh Renderer on object " + meshFilter.name + " has no material assigned. Skipping.");
                continue;
            }

            // If there are more materials than submeshes, cancel.
            if (materials.Length > meshFilter.sharedMesh.subMeshCount)
            {
                // Rollback: return the object to original position
                Debug.LogError("Mesh Combine Wizard: Objects with multiple materials on the same mesh are not supported. Create multiple meshes from this object's sub-meshes in an external 3D tool and assign separate materials to each. Operation cancelled.");
                return null;
            }

            var colors = meshFilter.sharedMesh.colors;
            var reflectionMatrix = Matrix4x4.identity;
            var tie = meshRenderer.GetComponentInParent<Tie>();
            var shrub = meshRenderer.GetComponentInParent<Shrub>();
            var tfrag = meshRenderer.GetComponentInParent<TfragChunk>();
            if (tie)
            {
                reflectionMatrix = tie.Reflection;
                var color = tie.GetBaseVertexColor().ScaleRGB(dzoConfig.TieBrightness * tie.DZOBrightness); // dzo expect vertex color RGB to be same as game, but alpha to be corrected without bloom
                colors = Enumerable.Repeat(color, meshFilter.sharedMesh.vertexCount).ToArray();
            }
            else if (shrub)
            {
                reflectionMatrix = shrub.Reflection;
                var color = shrub.Tint.ScaleRGB(dzoConfig.ShrubBrightness * shrub.DZOBrightness); // dzo expect vertex color RGB to be same as game, but alpha to be corrected without bloom
                colors = Enumerable.Repeat(color, meshFilter.sharedMesh.vertexCount).ToArray();
            }
            else if (tfrag)
            {
                for (int i = 0; i < colors.Length; ++i)
                    colors[i] = colors[i].ScaleRGB(dzoConfig.TfragBrightness * tfrag.DZOBrightness);
            }

            // create a clone of the mesh and bake any vertex colors
            var meshWithColors = new Mesh()
            {
                vertices = meshFilter.sharedMesh.vertices,
                normals = meshFilter.sharedMesh.normals,
                uv = meshFilter.sharedMesh.uv,
                tangents = meshFilter.sharedMesh.tangents,
                colors = colors,
                subMeshCount = meshFilter.sharedMesh.subMeshCount,
                indexFormat = meshFilter.sharedMesh.indexFormat,
                boneWeights = meshFilter.sharedMesh.boneWeights,
                bindposes = meshFilter.sharedMesh.bindposes,
                bounds = meshFilter.sharedMesh.bounds,
            };

            for (int i = 0; i < meshWithColors.subMeshCount; ++i)
            {
                var triangles = meshFilter.sharedMesh.GetTriangles(i);
                meshWithColors.SetTriangles(triangles, i);
            }

            for (int i = 0; i < materials.Length; ++i)
            {
                var material = materials[i];
                var entry = new MeshFilterSubMesh()
                {
                    Mesh = meshWithColors,
                    SubmeshIdx = i,
                    WorldMatrix = reflectionMatrix * meshFilter.transform.localToWorldMatrix
                };

                // Add material to mesh filter mapping to dictionary
                if (materialToMeshFilterList.ContainsKey(material)) materialToMeshFilterList[material].Add(entry);
                else materialToMeshFilterList.Add(material, new List<MeshFilterSubMesh>() { entry });
            }
        }

        // For each material, create a new merged object, in the scene and in the assets.
        foreach (var entry in materialToMeshFilterList)
        {
            List<MeshFilterSubMesh> meshesWithSameMaterial = entry.Value;

            // Create a convenient material name
            string materialName = entry.Key.ToString().Split(' ')[0];

            CombineInstance[] combine = new CombineInstance[meshesWithSameMaterial.Count];
            for (int i = 0; i < meshesWithSameMaterial.Count; i++)
            {
                combine[i].mesh = meshesWithSameMaterial[i].Mesh;
                combine[i].transform = meshesWithSameMaterial[i].WorldMatrix;
                combine[i].subMeshIndex = meshesWithSameMaterial[i].SubmeshIdx;
            }

            // Create a new mesh using the combined properties
            var format = true ? IndexFormat.UInt32 : IndexFormat.UInt16;
            Mesh combinedMesh = new Mesh { indexFormat = format };
            combinedMesh.CombineMeshes(combine);

            // Create asset
            materialName += "_" + combinedMesh.GetInstanceID();
            string goName = (entry.Key.shader == universalShader ? "dzo." : "misc.") + ((materialToMeshFilterList.Count > 1) ? "CombinedMeshes_" + materialName : "CombinedMeshes");
            var meshMetadata = new DzoMapMetadata.StaticMeshMetadata() { Name = goName };

            //if (generateSecondaryUVs)
            //{
            //    Unwrapping.GenerateSecondaryUVSet(combinedMesh);
            //}

            // try and convert Universal shader to Standard, so gltf can export correctly
            Material newMat = null;
            if (entry.Key.shader == universalShader)
            {
                newMat = new Material(Shader.Find("Standard"));
                newMat.SetTexture("_MainTex", entry.Key.GetTexture("_MainTex"));
                newMat.SetTexture("_EmissionMap", entry.Key.GetTexture("_MainTex"));
                newMat.SetColor("_Color", entry.Key.GetColor("_Color"));
                newMat.SetFloat("_Cutoff", entry.Key.GetFloat("_AlphaClip"));
                newMat.SetFloat("_Mode", 0);
                newMat.SetFloat("_Glossiness", entry.Key.GetFloat("_Smoothness"));

                if (entry.Key.GetInteger("_Transparent") > 0)
                {
                    newMat.SetOverrideTag("RenderType", "Transparent");
                    newMat.SetInt("_SrcBlend", (int)UnityEngine.Rendering.BlendMode.SrcAlpha);
                    newMat.SetInt("_DstBlend", (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
                    newMat.SetInt("_SrcBlendAlpha", (int)UnityEngine.Rendering.BlendMode.SrcAlpha);
                    newMat.SetInt("_DstBlendAlpha", (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
                    newMat.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Transparent;
                    newMat.SetFloat("_Mode", 3);
                }
                else if (entry.Key.GetFloat("_AlphaClip") > 0)
                {
                    newMat.SetOverrideTag("RenderType", "TransparentCutout");
                    newMat.renderQueue = (int)UnityEngine.Rendering.RenderQueue.AlphaTest;
                    newMat.SetFloat("_Mode", 1);
                }
            }
            else
            {
                newMat = new Material(entry.Key);
            }

            // store emission
            if (entry.Key.IsKeywordEnabled("_EMISSION") || (entry.Key.HasInteger("_Emission") && entry.Key.GetInteger("_Emission") > 0))
            {
                float intensity = entry.Key.GetColor("_EmissionColor").maxColorComponent;
                newMat.EnableKeyword("_EMISSION");
                newMat.SetColor("_EmissionColor", entry.Key.GetColor("_EmissionColor") / intensity);

                meshMetadata.EmissiveIntensity = intensity;
            }

            // Create game object
            GameObject combinedObject = new GameObject(goName);
            var filter = combinedObject.AddComponent<MeshFilter>();
            filter.sharedMesh = combinedMesh;
            var renderer = combinedObject.AddComponent<MeshRenderer>();
            renderer.sharedMaterial = newMat;
            combinedObjects.Add(combinedObject);
            metadata.Meshes.Add(meshMetadata);
        }

        // If there was more than one material, and thus multiple GOs created, parent them and work with result
        GameObject resultGO = null;
        if (combinedObjects.Count > 1)
        {
            resultGO = new GameObject("combined");
            foreach (var combinedObject in combinedObjects) combinedObject.transform.parent = resultGO.transform;
        }
        else if (combinedObjects.Count == 1)
        {
            resultGO = combinedObjects[0];
        }

        // Disable the original and return both to original positions
        return resultGO;
    }

    class MeshFilterSubMesh
    {
        public Mesh Mesh { get; set; }
        public int SubmeshIdx { get; set; }
        public Matrix4x4 WorldMatrix { get; set; } = Matrix4x4.identity;
    }
}
