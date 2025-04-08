using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(TfragChunk)), CanEditMultipleObjects]
public class TfragChunkEditor : Editor
{
    private void OnEnable()
    {
    }

    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        // occlusion
        var octantCount = targets.Sum(x => (x as TfragChunk)?.Octants?.Length ?? 0);

        GUILayout.Label("");
        GUILayout.Label($"Occlusion ({octantCount} Total Octants)", EditorStyles.boldLabel);
        TfragChunk.RenderOctants = GUILayout.Toggle(TfragChunk.RenderOctants, "Render Octants");
        GUILayout.Label("");

        // set to all octants
        if (GUILayout.Button($"Set To All Octants"))
        {
            var octants = UnityHelper.GetAllOctants();

            Undo.RecordObjects(targets, "Set To All Octants");
            foreach (var obj in targets)
            {
                if (obj is TfragChunk chunk)
                {
                    chunk.Octants = octants.ToArray();
                }
            }
            Undo.FlushUndoRecordObjects();
        }

        GUILayout.BeginHorizontal();
        if (GUILayout.Button("Copy Octants"))
        {
            OcclusionBaker.ClipboardOcclusionData = (target as TfragChunk)?.Octants?.ToArray() ?? new Vector3[0];
        }
        EditorGUI.BeginDisabledGroup(OcclusionBaker.ClipboardOcclusionData == null);
        if (GUILayout.Button("Paste Octants" + (OcclusionBaker.ClipboardOcclusionData != null ? $" ({OcclusionBaker.ClipboardOcclusionData.Length})" : "")))
        {
            var data = OcclusionBaker.ClipboardOcclusionData;
            Undo.RecordObjects(targets, "Paste Octants");
            foreach (TfragChunk tfrag in targets)
            {
                tfrag.Octants = data.ToArray();
            }
            Undo.FlushUndoRecordObjects();
        }
        EditorGUI.EndDisabledGroup();
        GUILayout.EndHorizontal();

        // clear octants
        if (GUILayout.Button($"Clear Octants"))
        {
            Undo.RecordObjects(targets, "Clear Octants");
            foreach (var obj in targets)
            {
                if (obj is TfragChunk chunk)
                {
                    chunk.Octants = new Vector3[0];
                }
            }
            Undo.FlushUndoRecordObjects();
        }
    }
}
