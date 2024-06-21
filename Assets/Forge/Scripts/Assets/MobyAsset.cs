using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

[ExecuteInEditMode]
public class MobyAsset : MonoBehaviour
{
    private void Update()
    {
        // we never want an instance of the asset
        // we want a reference to the asset
        if (PrefabStageUtility.GetCurrentPrefabStage() != null) return;
        if (this.hideFlags.HasFlag(HideFlags.HideInInspector)) return;
        if (this.hideFlags.HasFlag(HideFlags.DontSave)) return;
        if (this.hideFlags.HasFlag(HideFlags.HideInHierarchy)) return;

        if (!int.TryParse(this.name.Split(' ')[0], out var oclass))
        {
            DestroyImmediate(this.gameObject);
            return;
        }

        var prefab = PrefabUtility.GetCorrespondingObjectFromSource(this.gameObject);
        var assetPath = AssetDatabase.GetAssetPath(prefab);
        var assetDir = Path.GetDirectoryName(assetPath);

        var assetLabels = AssetDatabase.GetLabels(prefab);
        var racVersion = 0;
        if (assetLabels != null)
        {
            foreach (var assetLabel in assetLabels)
            {
                if (assetLabel == "GC")
                    racVersion = 2;
                else if (assetLabel == "UYA")
                    racVersion = 3;
                else if (assetLabel == "DL")
                    racVersion = 4;
            }
        }

        if (racVersion == 0)
        {
            Debug.LogError($"Unable to determine which game {this.gameObject.name} belongs to.");
            DestroyImmediate(this.gameObject);
            return;
        }
        else if (racVersion == 3 && racVersion != 4)
        {
            Debug.LogError($"Moby {this.gameObject.name} belongs to an unsupported game rc{racVersion}");
            DestroyImmediate(this.gameObject);
            return;
        }

        // check if oclass exists in local map
        var localMobyAssetFolder = Path.Combine(FolderNames.GetLocalAssetFolder(FolderNames.MobyFolder), oclass.ToString());
        if (!Directory.Exists(localMobyAssetFolder))
        {
            // 
            if (!EditorUtility.DisplayDialog($"The moby {oclass} has not yet been imported into the map", "Would you like to import it?", "Yes", "Cancel"))
            {
                DestroyImmediate(this.gameObject);
                return;
            }

            // import
            try
            {
                PackerHelper.DuplicateAsset(assetDir, localMobyAssetFolder, FolderNames.MobyFolder);
            }
            catch (Exception e)
            {
                Debug.LogError(e);
                DestroyImmediate(this.gameObject);
                return;
            }
        }

        var go = new GameObject(this.name);
        var moby = go.AddComponent<Moby>();
        moby.transform.position = this.transform.position;
        moby.transform.rotation = this.transform.rotation;
        moby.transform.localScale = this.transform.localScale;
        moby.OClass = oclass;
        moby.RCVersion = racVersion;
        moby.InitializePVarReferences(true);

        if (Selection.activeGameObject == this.gameObject)
            Selection.activeGameObject = go;

        DestroyImmediate(this.gameObject);
    }

}
