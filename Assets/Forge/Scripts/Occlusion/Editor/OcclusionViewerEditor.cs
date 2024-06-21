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
        GUILayout.Label(occlusionViewer.VisibleCount.ToString());
    }
}
