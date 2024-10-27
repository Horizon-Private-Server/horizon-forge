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
            ExportGraphsAsC();
        }
    }

    private void ExportGraphsAsC()
    {
        int i = 0;
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        var graphs = mapConfig.GetPathGraphs();

        var sb = new StringBuilder();
        var sbPathGraphDefs = new StringBuilder();

        sb.AppendLine("#include <libdl/math3d.h>");
        sb.AppendLine("#include \"pathfind.h\"");
        sb.AppendLine();

        sbPathGraphDefs.AppendLine("struct PathGraph Paths[] = {");
        foreach (var graph in graphs)
        {
            graph.ExportAsC($"MOB{i}", out var dataDefs, out var pathGraphDefs);
            sb.AppendLine(dataDefs);
            sbPathGraphDefs.AppendLine(pathGraphDefs);
            ++i;
        }
        sb.AppendLine($"const int PathsCount = {i};");
        sbPathGraphDefs.AppendLine("};");

        EditorGUIUtility.systemCopyBuffer = sb.ToString() + sbPathGraphDefs.ToString();
    }
}
