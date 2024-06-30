using GLTFast.Export;
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

public static class CollisionBaker
{
    static string _lastSaveFilePath = null;

    [MenuItem("Forge/Tools/Collision/Bake Collision")]
    public static async Task<bool> BakeCollision()
    {
        var scene = SceneManager.GetActiveScene();
        if (scene == null) return false;

        var resourcesFolder = FolderNames.GetMapFolder(scene.name);
        var binFolder = FolderNames.GetMapBinFolder(scene.name, Constants.GameVersion);
        var collisionBlendFile = Path.Combine(resourcesFolder, "collision.blend");
        var collisionDaeFile = Path.Combine(binFolder, FolderNames.BinaryCollisionColladaFile);
        var collisionAssetFile = Path.Combine(binFolder, FolderNames.BinaryCollisionAssetFile);
        var collisionBinFile = Path.Combine(binFolder, FolderNames.BinaryCollisionBinFile);
        var affectedInstancedColliders = new List<CollisionRenderHandle>();
        GameObject combinedRootGo = null;

        var exportSettings = new ExportSettings
        {
            Format = GltfFormat.Binary,
            ImageDestination = ImageDestination.MainBuffer,
            FileConflictResolution = FileConflictResolution.Overwrite,
            ComponentMask = GLTFast.ComponentType.Mesh
        };

        var gameObjectExportSettings = new GameObjectExportSettings
        {
            OnlyActiveInHierarchy = true,
            DisabledComponents = true,
        };

        try
        {
            EditorUtility.DisplayProgressBar("Baking Collision", "Collecting Colliders", 0.25f);

            // build instanced collision
            var instancedColliders = new List<IInstancedCollider>();
            instancedColliders.AddRange(GameObject.FindObjectsOfType<Tie>(includeInactive: false) ?? new Tie[0]);
            instancedColliders.AddRange(GameObject.FindObjectsOfType<Shrub>(includeInactive: false) ?? new Shrub[0]);
            instancedColliders.AddRange(GameObject.FindObjectsOfType<InstancedMeshCollider>(includeInactive: false) ?? new InstancedMeshCollider[0]);

            foreach (var instance in instancedColliders)
            {
                if (instance.HasInstancedCollider())
                {
                    // ensure asset is up to date
                    if (instance is IAsset asset) asset.UpdateAsset();

                    var instancedCollider = instance.GetInstancedCollider();
                    if (instancedCollider == null || !instancedCollider.AssetInstance) continue;

                    // move collider to collision bake root temporarily for export
                    instancedCollider.OnPreBake();
                    affectedInstancedColliders.Add(instancedCollider);
                }
            }

            // export and merge instanced collision
            EditorUtility.DisplayProgressBar("Baking Collision", "Merging Colliders", 0.5f);
            var export = new GameObjectExport(exportSettings, gameObjectExportSettings);

            // merge
            combinedRootGo = CombineMeshes(affectedInstancedColliders.Select(x => x.AssetInstance).ToArray());

            // export scene
            combinedRootGo.transform.rotation = Quaternion.AngleAxis(180f, Vector3.up);
            export.AddScene(new GameObject[] { combinedRootGo });

            // save glb file
            var outInstancedCollisionGlbFile = Path.Combine(FolderNames.GetTempFolder(), $"instanced-collision.glb");
            if (File.Exists(outInstancedCollisionGlbFile)) File.Delete(outInstancedCollisionGlbFile);
            using (var fs = File.Create(outInstancedCollisionGlbFile))
            {
                if (!await export.SaveToStreamAndDispose(fs))
                {
                    return false;
                }
            }

            // merge and export as collada
            EditorUtility.DisplayProgressBar("Baking Collision", "Baking Collision", 0.75f);
            if (!BlenderHelper.PackCollision(collisionBlendFile, collisionDaeFile, outInstancedCollisionGlbFile))
            {
                Debug.LogError($"Failed to export collision blend as collada {collisionDaeFile}");
                return false;
            }

            // build collision
            EditorUtility.DisplayProgressBar("Baking Collision", "Packing Collision", 0.9f);
            if (!WrenchHelper.BuildCollision(Path.Combine(Environment.CurrentDirectory, collisionAssetFile), Path.Combine(Environment.CurrentDirectory, collisionBinFile)))
            {
                Debug.LogError($"Failed to pack collision. Please make sure all faces are quads/tris and that all mesh data exists in the positive quadrant");
                return false;
            }

            Debug.Log("Collision successfully baked!");
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
            return false;
        }
        finally
        {
            // post bake
            foreach (var instance in affectedInstancedColliders)
                instance.OnPostBake();

            // remove temporary collision bake root
            if (combinedRootGo) GameObject.DestroyImmediate(combinedRootGo);

            EditorUtility.ClearProgressBar();
        }

        return true;
    }

    [MenuItem("Forge/Tools/Collision/Export Selected Instanced Collision")]
    public static async Task<bool> ExportSelectedInstancedCollision()
    {
        var scene = SceneManager.GetActiveScene();
        if (scene == null) return false;

        // save glb file
        var savePath = EditorUtility.SaveFilePanel("", string.IsNullOrEmpty(_lastSaveFilePath) ? "" : Path.GetDirectoryName(_lastSaveFilePath), string.IsNullOrEmpty(_lastSaveFilePath) ? "" : Path.GetFileName(_lastSaveFilePath), "glb");
        if (string.IsNullOrEmpty(savePath))
            return false;
        _lastSaveFilePath = savePath;

        var resourcesFolder = FolderNames.GetMapFolder(scene.name);
        var binFolder = FolderNames.GetMapBinFolder(scene.name, Constants.GameVersion);
        var affectedInstancedColliders = new List<CollisionRenderHandle>();
        GameObject combinedRootGo = null;

        var exportSettings = new ExportSettings
        {
            Format = GltfFormat.Binary,
            ImageDestination = ImageDestination.MainBuffer,
            FileConflictResolution = FileConflictResolution.Overwrite,
            ComponentMask = GLTFast.ComponentType.Mesh
        };

        var gameObjectExportSettings = new GameObjectExportSettings
        {
            OnlyActiveInHierarchy = true,
            DisabledComponents = true,
        };

        try
        {
            EditorUtility.DisplayProgressBar("Exporting Instanced Collision", "Collecting Colliders", 0.25f);

            var selectedGameObjects = Selection.gameObjects;

            // build instanced collision
            var instancedColliders = new List<IInstancedCollider>();
            foreach (var selectedGameObject in selectedGameObjects)
            {
                instancedColliders.AddRange(selectedGameObject.GetComponentsInChildren<Tie>(includeInactive: false) ?? new Tie[0]);
                instancedColliders.AddRange(selectedGameObject.GetComponentsInChildren<Shrub>(includeInactive: false) ?? new Shrub[0]);
                instancedColliders.AddRange(selectedGameObject.GetComponentsInChildren<InstancedMeshCollider>(includeInactive: false) ?? new InstancedMeshCollider[0]);
            }

            foreach (var instance in instancedColliders)
            {
                if (instance.HasInstancedCollider())
                {
                    // ensure asset is up to date
                    if (instance is IAsset asset) asset.UpdateAsset();

                    var instancedCollider = instance.GetInstancedCollider();
                    if (instancedCollider == null || !instancedCollider.AssetInstance) continue;

                    // move collider to collision bake root temporarily for export
                    instancedCollider.OnPreBake();
                    affectedInstancedColliders.Add(instancedCollider);
                }
            }

            // export and merge instanced collision
            EditorUtility.DisplayProgressBar("Exporting Instanced Collision", "Merging Colliders", 0.5f);
            var export = new GameObjectExport(exportSettings, gameObjectExportSettings);

            // merge
            combinedRootGo = CombineMeshes(affectedInstancedColliders.Select(x => x.AssetInstance).ToArray());

            // export scene
            combinedRootGo.transform.rotation = Quaternion.AngleAxis(180f, Vector3.up);
            export.AddScene(new GameObject[] { combinedRootGo });

            if (File.Exists(_lastSaveFilePath)) File.Delete(_lastSaveFilePath);
            using (var fs = File.Create(_lastSaveFilePath))
            {
                if (!await export.SaveToStreamAndDispose(fs))
                {
                    return false;
                }
            }

            Debug.Log("Selected Instanced Collision successfully exported!");
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
            return false;
        }
        finally
        {
            // post bake
            foreach (var instance in affectedInstancedColliders)
                instance.OnPostBake();

            // remove temporary collision bake root
            if (combinedRootGo) GameObject.DestroyImmediate(combinedRootGo);

            EditorUtility.ClearProgressBar();
        }

        return true;
    }

    static GameObject CombineMeshes(GameObject[] gameObjects)
    {
        // Locals
        Dictionary<int, List<MeshFilterSubMesh>> colIdToMeshFilterList = new Dictionary<int, List<MeshFilterSubMesh>>();
        List<GameObject> combinedObjects = new List<GameObject>();

        MeshFilter[] meshFilters = gameObjects.SelectMany(x => x.GetComponentsInChildren<MeshFilter>()).ToArray();

        // Go through all mesh filters and establish the mapping between the materials and all mesh filters using it.
        foreach (var meshFilter in meshFilters)
        {
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
            if (tie)
            {
                reflectionMatrix = tie.Reflection;
                var color = tie.GetBaseVertexColor().HalveRGB(); // dzo expect vertex color RGB to be same as game, but alpha to be corrected without bloom
                colors = Enumerable.Repeat(color, meshFilter.sharedMesh.vertexCount).ToArray();
            }
            else if (shrub)
            {
                reflectionMatrix = shrub.Reflection;
                var color = shrub.Tint.HalveRGB(); // dzo expect vertex color RGB to be same as game, but alpha to be corrected without bloom
                colors = Enumerable.Repeat(color, meshFilter.sharedMesh.vertexCount).ToArray();
            }

            // create a clone of the mesh and bake any vertex colors
            var meshCopy = new Mesh()
            {
                vertices = meshFilter.sharedMesh.vertices,
                normals = meshFilter.sharedMesh.normals,
                uv = meshFilter.sharedMesh.uv,
                tangents = meshFilter.sharedMesh.tangents,
                subMeshCount = meshFilter.sharedMesh.subMeshCount,
                indexFormat = meshFilter.sharedMesh.indexFormat,
                bounds = meshFilter.sharedMesh.bounds,
            };

            for (int i = 0; i < meshCopy.subMeshCount; ++i)
            {
                var triangles = meshFilter.sharedMesh.GetTriangles(i);
                meshCopy.SetTriangles(triangles, i);
            }

            for (int i = 0; i < materials.Length; ++i)
            {
                var material = materials[i];
                if (material.shader.name != "Horizon Forge/Collider")
                {
                    Debug.LogWarning($"Collision Baker: ignoring bad collider material {material.name} with shader {material.shader.name} for object {meshFilter.gameObject.name}");
                    continue;
                }

                var entry = new MeshFilterSubMesh()
                {
                    Mesh = meshCopy,
                    SubmeshIdx = i,
                    WorldMatrix = reflectionMatrix * meshFilter.transform.localToWorldMatrix
                };

                var colId = material.GetInteger("_ColId");

                // Add material to mesh filter mapping to dictionary
                if (colIdToMeshFilterList.ContainsKey(colId)) colIdToMeshFilterList[colId].Add(entry);
                else colIdToMeshFilterList.Add(colId, new List<MeshFilterSubMesh>() { entry });
            }
        }

        // For each material, create a new merged object, in the scene and in the assets.
        foreach (var entry in colIdToMeshFilterList)
        {
            List<MeshFilterSubMesh> meshesWithSameMaterial = entry.Value;

            // create mat name from col id
            string materialName = "col_" + entry.Key.ToString("x");

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

            //if (generateSecondaryUVs)
            //{
            //    Unwrapping.GenerateSecondaryUVSet(combinedMesh);
            //}

            // try and convert Universal shader to Standard, so gltf can export correctly
            var newMat = new Material(Shader.Find("Standard"));
            newMat.SetColor("_Color", CollisionHelper.GetColor(entry.Key));

            // Create asset
            newMat.name = materialName;

            // Create game object
            string goName = materialName;
            GameObject combinedObject = new GameObject(goName);
            var filter = combinedObject.AddComponent<MeshFilter>();
            filter.sharedMesh = combinedMesh;
            var renderer = combinedObject.AddComponent<MeshRenderer>();
            renderer.sharedMaterial = newMat;
            combinedObjects.Add(combinedObject);
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
