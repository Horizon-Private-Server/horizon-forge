using GLTFast.Export;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class CollisionBaker
{

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
        var rootGo = new GameObject("collisionbake");
        var reparentGos = new List<(GameObject, Transform)>();
        var affectedInstancedColliders = new List<CollisionRenderHandle>();

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
                    reparentGos.Add((instancedCollider.AssetInstance, instancedCollider.AssetInstance.transform.parent));
                    instancedCollider.AssetInstance.transform.SetParent(rootGo.transform, true);
                    instancedCollider.OnPreBake();
                    affectedInstancedColliders.Add(instancedCollider);
                }
            }

            // export and merge instanced collision
            EditorUtility.DisplayProgressBar("Baking Collision", "Merging Colliders", 0.5f);
            var export = new GameObjectExport(exportSettings, gameObjectExportSettings);

            // export scene
            rootGo.transform.rotation = Quaternion.AngleAxis(180f, Vector3.up);
            export.AddScene(new GameObject[] { rootGo });

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

            // move colliders back to original parents
            if (rootGo) rootGo.transform.rotation = Quaternion.identity;
            foreach (var reparent in reparentGos)
                reparent.Item1.transform.SetParent(reparent.Item2, true);

            // remove temporary collision bake root
            if (rootGo) GameObject.DestroyImmediate(rootGo);

            EditorUtility.ClearProgressBar();
        }

        return true;
    }

}
