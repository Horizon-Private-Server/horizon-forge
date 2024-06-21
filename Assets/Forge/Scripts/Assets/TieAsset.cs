using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode]
public class TieAsset : MonoBehaviour
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
        var ambientSize = 0;

        var assetPath = AssetDatabase.GetAssetPath(PrefabUtility.GetCorrespondingObjectFromSource(this.gameObject));
        var assetDir = Path.GetDirectoryName(assetPath);

        // check if oclass exists in local map
        var localTieAssetFolder = Path.Combine(FolderNames.GetLocalAssetFolder(FolderNames.TieFolder), oclass.ToString());
        if (!Directory.Exists(localTieAssetFolder))
        {
            // 
            if (!EditorUtility.DisplayDialog($"The tie {oclass} has not yet been imported into the map", "Would you like to import it?", "Yes", "Cancel"))
            {
                DestroyImmediate(this.gameObject);
                return;
            }

            // import
            try
            {
                PackerHelper.DuplicateAsset(assetDir, localTieAssetFolder, FolderNames.TieFolder);
            }
            catch (Exception e)
            {
                Debug.LogError(e);
                DestroyImmediate(this.gameObject);
                return;
            }
        }

        // determine color size
        var tieBinFilePath = Path.Combine(assetDir, "core.bin");
        if (File.Exists(tieBinFilePath))
        {
            var tieBytes = File.ReadAllBytes(tieBinFilePath);
            if (tieBytes != null && tieBytes.Length > 0x3C)
                ambientSize = BitConverter.ToInt16(tieBytes, 0x3a);
        }

        // create object
        var go = new GameObject(this.name);
        var tie = go.AddComponent<Tie>();
        tie.transform.position = this.transform.position;
        tie.transform.rotation = this.transform.rotation;
        tie.transform.localScale = this.transform.localScale;
        tie.OClass = oclass;
        tie.ColorData = new byte[ambientSize];
        GenerateUniformColor(tie, new Color(0.25f, 0.25f, 0.25f, 0.5f));

        // give unique occlusion id
        HashSet<int> existingIds = IOcclusionData.AllOcclusionDatas.Where(x => x.OcclusionType == OcclusionDataType.Tie).Select(x => x.OcclusionId).ToHashSet();
        var newOcclusionId = existingIds.FirstOrDefault();
        while (existingIds.Contains(newOcclusionId))
            newOcclusionId += 1;
        tie.OcclusionId = newOcclusionId;

        if (Selection.activeGameObject == this.gameObject)
            Selection.activeGameObject = go;

        DestroyImmediate(this.gameObject);
    }

    public static void GenerateUniformColor(Tie tie, Color color)
    {
        if (!tie || tie.ColorData == null) return;

        var tieColor = color;
        var tieMaskColor = Color.white;
        var colorBytes = tie.ColorData;
        if (colorBytes.Length >= 4)
        {
            colorBytes[0] = (byte)(tieColor.r * 255);
            colorBytes[1] = (byte)(tieColor.g * 255);
            colorBytes[2] = (byte)(tieColor.b * 255);
            colorBytes[3] = (byte)(tieColor.a * 255);
        }

        for (int i = 4; i < colorBytes.Length; i += 2)
        {
            colorBytes[i + 0] = (byte)(((uint)(tieMaskColor.a * 15) << 4) | ((uint)(tieMaskColor.r * 15) << 0));
            colorBytes[i + 1] = (byte)(((uint)(tieMaskColor.g * 15) << 4) | ((uint)(tieMaskColor.b * 15) << 0));
        }
    }

    public static byte[] GenerateUniformColor(int colorSize, Color color)
    {
        var tieColor = color;
        var tieMaskColor = Color.white;
        var colorBytes = new byte[colorSize];
        if (colorBytes.Length >= 4)
        {
            colorBytes[0] = (byte)(tieColor.r * 255);
            colorBytes[1] = (byte)(tieColor.g * 255);
            colorBytes[2] = (byte)(tieColor.b * 255);
            colorBytes[3] = 0xFF; // (byte)(tieColor.a * 255);
        }

        for (int i = 4; i < colorBytes.Length; i += 2)
        {
            colorBytes[i + 0] = (byte)(((uint)(tieMaskColor.a * 15) << 4) | ((uint)(tieMaskColor.r * 15) << 0));
            colorBytes[i + 1] = (byte)(((uint)(tieMaskColor.g * 15) << 4) | ((uint)(tieMaskColor.b * 15) << 0));
        }

        return colorBytes;
    }
}
