using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

public class OcclusionDatabase : ScriptableObject
{
    public OcclusionDictionary Occlusion = new OcclusionDictionary();

    private void OnEnable()
    {
        Undo.undoRedoPerformed -= UndoRedoPerformed;
        Undo.undoRedoPerformed += UndoRedoPerformed;
        EditorSceneManager.sceneSaving -= OnSceneSaving;
        EditorSceneManager.sceneSaving += OnSceneSaving;
        ReconcileScene();
    }

    void UndoRedoPerformed()
    {
        ReconcileScene();
    }

    void OnSceneSaving(Scene scene, string path)
    {
        if (scene != EditorSceneManager.GetActiveScene()) return;

        ReconcileScene();
    }

    #region Accessors

    public OcclusionData Get(IOcclusionData occlusion)
    {
        var key = occlusion.Uid.ToString();
        var data = Occlusion.GetValueOrDefault(key);
        if (data) return data;

        data = TryRead(key);
        if (data)
        {
            Occlusion.Add(key, data);
            return data;
        }

        return null;
    }

    public OcclusionData GetOrCreate(IOcclusionData occlusion)
    {
        var key = occlusion.Uid.ToString();
        var data = Get(occlusion);
        if (data) return data;

        return Create(occlusion);
    }

    public OcclusionData Create(IOcclusionData occlusion)
    {
        if (occlusion == null)
        {
            Debug.LogError("Trying to create occlusion data for object without IOcclusionData");
            return null;
        }

        if (occlusion.Uid == Guid.Empty)
        {
            Debug.LogError("Trying to create occlusion data for object with empty Uid");
            return null;
        }

        var mapFolder = FolderNames.GetMapFolder(SceneManager.GetActiveScene().name);
        var occlusionFolder = Path.Combine(mapFolder, FolderNames.OcclusionFolder);
        if (!Directory.Exists(occlusionFolder)) Directory.CreateDirectory(occlusionFolder);
        var occlFile = Path.Combine(occlusionFolder, $"{occlusion.Uid}.asset");
        var data = ScriptableObject.CreateInstance<OcclusionData>();
        data.Octants = occlusion.Octants;
        occlusion.Octants = new Vector3[0]; // remove old occlusion data
        EditorUtility.SetDirty((occlusion as MonoBehaviour).gameObject);

        AssetDatabase.CreateAsset(data, occlFile);
        AssetDatabase.SaveAssets();

        Undo.RecordObject(this, "Add Occlusion Data");
        Occlusion.Add(occlusion.Uid.ToString(), data);
        EditorUtility.SetDirty(this);
        return data;
    }

    public void BulkCreate(IEnumerable<IOcclusionData> occlusions)
    {
        if (occlusions == null) return;

        var mapFolder = FolderNames.GetMapFolder(SceneManager.GetActiveScene().name);
        var occlusionFolder = Path.Combine(mapFolder, FolderNames.OcclusionFolder);
        if (!Directory.Exists(occlusionFolder)) Directory.CreateDirectory(occlusionFolder);

        AssetDatabase.StartAssetEditing();
        //Undo.RecordObject(this, "Bulk Add Occlusion Data");

        try
        {
            foreach (var occlusion in occlusions)
            {
                if (occlusion.Uid == Guid.Empty)
                {
                    Debug.LogError("Trying to create occlusion data for object with empty Uid");
                    continue;
                }

                // already have 
                if (Get(occlusion)) continue;

                var occlFile = Path.Combine(occlusionFolder, $"{occlusion.Uid}.asset");
                var data = ScriptableObject.CreateInstance<OcclusionData>();
                data.Octants = occlusion.Octants;
                occlusion.Octants = new Vector3[0]; // remove old occlusion data
                EditorUtility.SetDirty((occlusion as MonoBehaviour).gameObject);

                AssetDatabase.CreateAsset(data, occlFile);
                Occlusion.Add(occlusion.Uid.ToString(), data);
            }
        }
        finally
        {
            EditorUtility.SetDirty(this);
            AssetDatabase.StopAssetEditing();
            AssetDatabase.SaveAssets();
        }
    }

    public bool Remove(IOcclusionData occlusion)
    {
        Undo.RecordObject(this, "Remove Occlusion Data");
        var removed = Occlusion.Remove(occlusion.Uid.ToString());
        EditorUtility.SetDirty(this);
        return removed;
    }

    public void ReconcileScene()
    {
        var occlusions = IOcclusionData.AllOcclusionDatas;
        var sceneOcclusionUids = occlusions.Select(x => x.Uid.ToString()).ToHashSet();
        var existingKeys = Occlusion.Keys.ToArray();
        var pendingRemoval = new List<string>();
        var pendingAddition = new Dictionary<string, OcclusionData>();

        // remove occlusion records not in scene
        foreach (var existingKey in existingKeys)
        {
            if (!sceneOcclusionUids.Contains(existingKey))
            {
                pendingRemoval.Add(existingKey);
            }
        }

        // add occlusion records that exist in scene but not in reference dictionary
        // we don't want to create new occlusion, only add if an existing OcclusionData ScriptableObject exists
        // the use-case is that when deleting a TfragChunk/Tie, removing its record in the dictionary, and then undoing, we should link back to the original occlusion data.
        foreach (var sceneOcclusionUid in sceneOcclusionUids)
        {
            if (Occlusion.ContainsKey(sceneOcclusionUid)) continue;

            // occlusion isn't in data
            // try and read the existing file
            var existingOcclusionData = TryRead(sceneOcclusionUid);
            if (!existingOcclusionData) continue;

            // add
            pendingAddition[sceneOcclusionUid] = existingOcclusionData;
        }

        if (pendingRemoval.Count == 0 && pendingAddition.Count == 0) return;

        foreach (var keyToRemove in pendingRemoval)
            Occlusion.Remove(keyToRemove);
        foreach (var occToAdd in pendingAddition)
            Occlusion.Add(occToAdd.Key, occToAdd.Value);
        EditorUtility.SetDirty(this);
    }

    public bool SetOctants(IOcclusionData occlusion, Vector3[] octants, bool recordUndo = false)
    {
        var data = GetOrCreate(occlusion);
        if (data == null) return false;

        if (recordUndo) Undo.RecordObject(data, "Set Occlusion Data");
        data.Octants = octants;
        EditorUtility.SetDirty(data);
        return true;
    }

    public bool SetOctants(IEnumerable<IOcclusionData> occlusions, Vector3[] octants, bool recordUndo = false)
    {
        var datas = occlusions.Select(x => GetOrCreate(x)).Where(x => x != null).Distinct().ToArray();
        if (datas.Length == 0) return false;

        if (recordUndo) Undo.RecordObjects(datas, "Set Occlusion Data");
        foreach (var data in datas)
        {
            data.Octants = octants.ToArray(); // save copy of octants
            EditorUtility.SetDirty(data);
        }

        return true;
    }

    #endregion

    private OcclusionData TryRead(string key)
    {
        var mapFolder = FolderNames.GetMapFolder(SceneManager.GetActiveScene().name);
        var occlusionFolder = Path.Combine(mapFolder, FolderNames.OcclusionFolder);
        if (!Directory.Exists(occlusionFolder)) return null;
        var occlFile = Path.Combine(occlusionFolder, $"{key}.asset");

        return AssetDatabase.LoadAssetAtPath<OcclusionData>(occlFile);
    }
}

[Serializable]
public class OcclusionDictionary : SerializableDictionary<string, OcclusionData>
{

}
