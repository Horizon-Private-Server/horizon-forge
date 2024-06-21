using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

[ExecuteInEditMode]
public class ShrubAsset : MonoBehaviour
{
    private void Update()
    {
        // we never want an instance of the asset
        // we want a reference to the asset
        if (PrefabStageUtility.GetCurrentPrefabStage() != null) return;
        if (this.hideFlags.HasFlag(HideFlags.HideInInspector)) return;
        if (this.hideFlags.HasFlag(HideFlags.DontSave)) return;
        if (this.hideFlags.HasFlag(HideFlags.HideInHierarchy)) return;

        var oclass = int.Parse(this.name.Split(' ')[0]);
        var assetPath = AssetDatabase.GetAssetPath(PrefabUtility.GetCorrespondingObjectFromSource(this.gameObject));
        var assetDir = Path.GetDirectoryName(assetPath);

        // check if oclass exists in local map
        var localShrubAssetFolder = Path.Combine(FolderNames.GetLocalAssetFolder(FolderNames.ShrubFolder), oclass.ToString());
        if (!Directory.Exists(localShrubAssetFolder))
        {
            // 
            if (!EditorUtility.DisplayDialog($"The shrub {oclass} has not yet been imported into the map", "Would you like to import it?", "Yes", "Cancel"))
            {
                DestroyImmediate(this.gameObject);
                return;
            }

            // import
            try
            {
                PackerHelper.DuplicateAsset(assetDir, localShrubAssetFolder, FolderNames.ShrubFolder);
            }
            catch (Exception e)
            {
                Debug.LogError(e);
                DestroyImmediate(this.gameObject);
                return;
            }
        }

        var go = new GameObject(this.name);
        var shrub = go.AddComponent<Shrub>();
        shrub.transform.position = this.transform.position;
        shrub.transform.rotation = this.transform.rotation;
        shrub.transform.localScale = this.transform.localScale;
        shrub.OClass = oclass;

        if (Selection.activeGameObject == this.gameObject)
            Selection.activeGameObject = go;

        DestroyImmediate(this.gameObject);
    }
}
