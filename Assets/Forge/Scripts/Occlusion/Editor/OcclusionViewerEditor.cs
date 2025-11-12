using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(OcclusionViewer))]
public class OcclusionViewerEditor : Editor
{
    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        var occlusionViewer = this.target as OcclusionViewer;

        GUILayout.Space(20);
        EditorGUILayout.HelpBox("Place this GameObject and click Enable to preview what objects are visible from that location.\n\nNote that due to occlusion compression, in game will likely have more objects visible. It will never have less.", MessageType.Info);

        GUILayout.Space(20);
        GUILayout.Label(occlusionViewer.VisibleCount.ToString());

        if (GUILayout.Button(occlusionViewer.enabled ? "Disable" : "Enable"))
        {
            occlusionViewer.enabled = !occlusionViewer.enabled;
        }
    }
}
