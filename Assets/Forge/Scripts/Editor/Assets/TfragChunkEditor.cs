using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(TfragChunk)), CanEditMultipleObjects]
public class TfragChunkEditor : Editor
{
    private MapConfig m_MapConfig;

    private void OnEnable()
    {
        m_MapConfig = FindObjectOfType<MapConfig>();
    }

    public override void OnInspectorGUI()
    {
        var octantCount = 0;

        base.OnInspectorGUI();

        // init db
        var db = m_MapConfig.GetOcclusionDatabase();
        foreach (var target in targets)
        {
            var data = db.GetOrCreate(target as TfragChunk);
            if (data is null) continue;

            octantCount += data.Octants?.Length ?? 0;
        }

        // occlusion
        GUILayout.Label("");
        GUILayout.Label($"Occlusion ({octantCount} Total Octants) (id={(target as TfragChunk).OcclusionId})", EditorStyles.boldLabel);
        TfragChunk.RenderOctants = GUILayout.Toggle(TfragChunk.RenderOctants, "Render Octants");
        GUILayout.Label("");

        // set to all octants
        if (GUILayout.Button($"Set To All Octants"))
        {
            var octants = UnityHelper.GetAllOctants();

            //Undo.RecordObjects(targets, "Set To All Octants");
            db.SetOctants(targets.Select(x => x as TfragChunk), octants.ToArray(), recordUndo: true);
            Undo.FlushUndoRecordObjects();
        }

        GUILayout.BeginHorizontal();
        if (GUILayout.Button("Copy Octants"))
        {
            var data = db.GetOrCreate(target as TfragChunk);
            OcclusionBaker.ClipboardOcclusionData = data?.Octants?.ToArray() ?? new Vector3[0];
        }
        EditorGUI.BeginDisabledGroup(OcclusionBaker.ClipboardOcclusionData == null);
        if (GUILayout.Button("Paste Octants" + (OcclusionBaker.ClipboardOcclusionData != null ? $" ({OcclusionBaker.ClipboardOcclusionData.Length})" : "")))
        {
            var data = OcclusionBaker.ClipboardOcclusionData;
            //Undo.RecordObjects(targets, "Paste Octants");
            db.SetOctants(targets.Select(x => x as TfragChunk), data, recordUndo: true);
            Undo.FlushUndoRecordObjects();
        }
        EditorGUI.EndDisabledGroup();
        GUILayout.EndHorizontal();

        // clear octants
        if (GUILayout.Button($"Clear Octants"))
        {
            //Undo.RecordObjects(targets, "Clear Octants");
            db.SetOctants(targets.Select(x => x as TfragChunk), new Vector3[0], recordUndo: true);
            Undo.FlushUndoRecordObjects();
        }
    }
}
