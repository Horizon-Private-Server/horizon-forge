using System.Collections;
using System.Collections.Generic;
using System.Linq; 
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(PathGraphNode))]
public class PathGraphNodeEditor : Editor
{
	private bool edgesFoldout = true;
    private Object otherNodeObjAdd;
    private Object otherNodeObjRemove;

    public override void OnInspectorGUI()
    {
        var node = this.serializedObject.targetObject as PathGraphNode;
        var graph = node.GetComponentInParent<PathGraph>();

        base.OnInspectorGUI();

        if (!graph)
            return;

        GUILayout.Space(20);
        if (GUILayout.Button("Recalculate Edges"))
        {
            Undo.RecordObject(graph, "recalculate node edges");
            node.RecalculateConnectedNodes();
        }

        GUILayout.BeginHorizontal();
        otherNodeObjAdd = EditorGUILayout.ObjectField(otherNodeObjAdd, typeof(PathGraphNode), allowSceneObjects: true);
        if (GUILayout.Button("Add Edge To") && otherNodeObjAdd && otherNodeObjAdd is PathGraphNode otherNodeAdd)
        {
            Undo.RecordObject(graph, "add node edge");
            node.AddEdge(otherNodeAdd);
        }
        GUILayout.EndHorizontal();

        GUILayout.BeginHorizontal();
        otherNodeObjRemove = EditorGUILayout.ObjectField(otherNodeObjRemove, typeof(PathGraphNode), allowSceneObjects: true);
        if (GUILayout.Button("Remove Edge To") && otherNodeObjRemove && otherNodeObjRemove is PathGraphNode otherNodeRemove)
        {
            Undo.RecordObject(graph, "remove node edge");
            node.RemoveEdge(otherNodeRemove);
        }
        GUILayout.EndHorizontal();
		
		
        edgesFoldout = EditorGUILayout.Foldout(edgesFoldout, "Connected Edges", true);

        if (edgesFoldout)
        {
            EditorGUI.indentLevel++;

            var edges = graph.Edges.Where(e => e.From == node || e.To == node).ToList();

            if (!edges.Any())
            {
                EditorGUILayout.LabelField("No connected edges");
            }
            else
            {
                foreach (var edge in edges)
                {
                    EditorGUILayout.BeginVertical("box");

                    EditorGUILayout.LabelField($"{edge.From.name} → {edge.To.name}", EditorStyles.boldLabel);

                    edge.CostFactor = EditorGUILayout.FloatField("Cost Factor", edge.CostFactor);
                    edge.PathFitStartEnd = EditorGUILayout.Slider("Path Fit Start/End", edge.PathFitStartEnd, 0f, 1f);

                    edge.Required = EditorGUILayout.Toggle("Required", edge.Required);
                    if (edge.Required)
                        edge.RequiredUntil = EditorGUILayout.Slider("Required Until", edge.RequiredUntil, 0f, 1f);

                    edge.JumpPad = EditorGUILayout.Toggle("JumpPad", edge.JumpPad);
                    if (edge.JumpPad)
                    {
                        edge.JumpPadSpeed = EditorGUILayout.FloatField("JumpPad Speed", edge.JumpPadSpeed);
                        edge.JumpPadAt = EditorGUILayout.Slider("JumpPad At", edge.JumpPadAt, 0f, 1f);
                    }

                    EditorGUILayout.EndVertical();
                }
            }

            EditorGUI.indentLevel--;
        }

        if (GUI.changed)
        {
            EditorUtility.SetDirty(graph);
        }
    }
}
