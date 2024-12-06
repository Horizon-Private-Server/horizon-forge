using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(PathGraphNode))]
public class PathGraphNodeEditor : Editor
{
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
    }
}
