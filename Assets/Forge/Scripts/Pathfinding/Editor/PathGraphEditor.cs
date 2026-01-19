using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(PathGraph))]
public class PathGraphEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var graph = this.serializedObject.targetObject as PathGraph;

        base.OnInspectorGUI();

        GUILayout.Space(20);

        var nodeCount = graph.GetNodeCount();
        if (nodeCount.HasValue)
        {
            GUILayout.Label("Nodes: " + nodeCount.Value.ToString());
        }
        
        if (GUILayout.Button("Refresh Cache"))
        {
            graph.RefreshCache();
        }

        if (GUILayout.Button("Export PathGraphs As C"))
        {
            EditorGUIUtility.systemCopyBuffer = PathGraph.ExportGraphsAsC();
        }
    }
}
