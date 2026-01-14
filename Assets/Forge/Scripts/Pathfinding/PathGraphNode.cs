using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Unity.VisualScripting;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class PathGraphNode : MonoBehaviour
{
    public float Radius = 1f;
    [Range(0f, 1f)] public float Cornering = 1f;
    public bool HasHeightLimit = false;
    [Min(0)] public float HeightLimit = 10f;

    private PathGraph _graph;
    private GUIStyle _weightStyle;
    private GUIStyle _edgeStyle;
    private Texture2D _requiredEdgeIcon;

    private void Start()
    {
        _weightStyle = null;
        _edgeStyle = null;
    }

    private void OnValidate()
    {
        _weightStyle = null;
        _edgeStyle = null;
    }

    public Vector3 GetCenterPosition() => this.transform.position + Vector3.up;
    public Color GetColor()
    {
        var graph = GetGraph();
        if (graph)
        {
            var idx = graph.GetNodes().IndexOf(this);
            if (idx >= 0)
            {
                return Color.HSVToRGB(idx / (float)graph.GetNodeCount(), 1, 1);
            }
        }

        return Color.green;
    }

    public void RecalculateConnectedNodes()
    {
        var graph = GetGraph();
        if (!graph)
            return;

        graph.RefreshCache();

        // make copy of existing edges before removing them
        var existingEdges = graph.Edges.Where(x => x.From == this || x.To == this).ToList();
        graph.Edges.RemoveAll(x => x.From == this || x.To == this);

        var allNodes = graph.GetNodes();
        foreach (var otherNode in allNodes)
        {
            if (otherNode == this)
                continue;

            if (graph.CanSeeNode(this, otherNode))
            {
                var existingFromTo = existingEdges.FirstOrDefault(x => x.From == this && x.To == otherNode);
                var existingToFrom = existingEdges.FirstOrDefault(x => x.To == this && x.From == otherNode);

                graph.Edges.Add(new PathGraphEdge() { From = this, To = otherNode, CostFactor = existingFromTo?.CostFactor ?? 1f, PathFitStartEnd = 0.5f });
                graph.Edges.Add(new PathGraphEdge() { To = this, From = otherNode, CostFactor = existingToFrom?.CostFactor ?? 1f, PathFitStartEnd = 0.5f });
            }
        }
        
    }

    public void AddEdge(PathGraphNode otherNode)
    {
        var graph = GetGraph();
        if (!graph)
            return;

        graph.RefreshCache();

        // make copy of existing edges before removing them
        var existingFrom = graph.Edges.FirstOrDefault(x => x.From == this && x.To == otherNode);
        var existingTo = graph.Edges.FirstOrDefault(x => x.To == this && x.From == otherNode);

        // add
        if (existingFrom == null)
        {
            graph.Edges.Add(new PathGraphEdge() { From = this, To = otherNode, CostFactor = 1f, PathFitStartEnd = 0.5f });
        }
        if (existingTo == null)
        {
            graph.Edges.Add(new PathGraphEdge() { To = this, From = otherNode, CostFactor = 1f, PathFitStartEnd = 0.5f });
        }
    }

    public void RemoveEdge(PathGraphNode otherNode)
    {
        var graph = GetGraph();
        if (!graph)
            return;

        graph.RefreshCache();

        graph.Edges.RemoveAll(x => (x.From == this || x.From == otherNode) && (x.To == this || x.To == otherNode));
    }

    private PathGraph GetGraph()
    {
        if (!_graph)
        {
            _graph = GetComponentInParent<PathGraph>();
            if (_graph)
                _graph.RefreshCache();
        }

        return _graph;
    }

    private void OnDestroy()
    {
        if (_graph)
        {
            // remove all edges referencing this node
            _graph.Edges.RemoveAll(x => x.From == this || x.To == this);
            _graph.RefreshCache();
        }
    }


    private void OnDrawGizmos()
    {
        if (!Selection.activeGameObject)
            return;

        if (!Selection.activeGameObject.TryGetComponent<PathGraphNode>(out _) && !Selection.activeGameObject.TryGetComponent<PathGraph>(out _))
            return;

        if (_weightStyle == null)
        {
            _weightStyle = new GUIStyle(GUI.skin.label);
            _weightStyle.fontSize = 16;
            _weightStyle.alignment = TextAnchor.MiddleCenter;
        }

        if (_edgeStyle == null)
        {
            _edgeStyle = new GUIStyle(GUI.skin.label);
            _edgeStyle.fontSize = 12;
            _edgeStyle.alignment = TextAnchor.MiddleCenter;
        }

        if (!_requiredEdgeIcon)
        {
            _requiredEdgeIcon = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Forge/Gizmos/Required Edge.png");
        }

        var graph = GetGraph();
        if (!graph)
            return;

        var myIdx = graph.GetNodeIndex(this);
        var cameraNearNode = SceneView.GetAllSceneCameras().Any(x => Vector3.Project((x.transform.position - GetCenterPosition()).normalized, x.transform.forward).magnitude > 0.8f);


        // draw node
        if (cameraNearNode)
            Handles.Label(GetCenterPosition() + Vector3.up * 0.1f, new GUIContent($"{this.name} ({graph.GetNodes().IndexOf(this)})"), _edgeStyle);

        var m = Gizmos.matrix;
        Gizmos.color = GetColor();
        Gizmos.matrix = Matrix4x4.TRS(GetCenterPosition(), this.transform.rotation, new Vector3(1, 0, 1));
        Gizmos.DrawWireSphere(Vector3.zero, Radius);
        Gizmos.matrix = Matrix4x4.TRS(GetCenterPosition(), this.transform.rotation, new Vector3(Cornering, 0, Cornering));
        Gizmos.DrawWireSphere(Vector3.zero, Radius);
        Gizmos.matrix = m;

        if (HasHeightLimit)
        {
            Gizmos.color = GetColor();
            Gizmos.matrix = Matrix4x4.TRS(GetCenterPosition() + (Vector3.up * HeightLimit), this.transform.rotation, new Vector3(1, 0, 1));
            Gizmos.DrawWireSphere(Vector3.zero, Radius);
            Gizmos.matrix = m;
            Gizmos.DrawLine(GetCenterPosition(), GetCenterPosition() + (Vector3.up * HeightLimit));
        }

        // draw connections
        foreach (var edge in graph.Edges.Where(x => x.From == this))
        {
            if (!edge.From || !edge.To || !edge.To.gameObject.activeInHierarchy)
                continue;

            // offset lines so that outgoing/incoming from same nodes don't overlap
            var start = edge.From.GetCenterPosition();
            var end = edge.To.GetCenterPosition();
            var sub = myIdx < graph.GetNodeIndex(edge.To);
            var offset = -Vector3.Cross((end - start).normalized, Vector3.up);
            var center = (start + offset + end + offset) / 2f;
            //if (sub)
            //    offset = -offset;

            var nearCenter = SceneView.GetAllSceneCameras().Any(x => Vector3.Distance(x.transform.position, center) < 20f);

            if (nearCenter)
            {
                Handles.Label(center + Vector3.up * 0.3f, new GUIContent($"w:{edge.CostFactor:0.00} f:{edge.PathFitStartEnd:0.00}"), _weightStyle);
            }

            if (nearCenter || SceneView.GetAllSceneCameras().Any(x => Vector3.Project((x.transform.position - center).normalized, x.transform.forward).magnitude > 0.9f))
                Handles.Label(center + Vector3.up * 0.1f, new GUIContent($"#{graph.Edges.IndexOf(edge)}"), _edgeStyle);

            Gizmos.DrawLine(start + offset, end + offset);
            
            if (edge.JumpPad)
            {
                Gizmos.DrawSphere(start + (end - start) * edge.JumpPadAt + offset, 0.25f);
            }

            if (edge.Required && _requiredEdgeIcon)
            {
                Vector3 pos = start + (end - start) * edge.RequiredUntil + offset;
                Vector3 guiPos = HandleUtility.WorldToGUIPoint(pos);

                float size = 32f;
                Rect rect = new Rect(
                    guiPos.x - size / 2f,
                    guiPos.y - size / 2f,
                    size,
                    size
                );

                Handles.BeginGUI();
                GUI.DrawTexture(rect, _requiredEdgeIcon);
                Handles.EndGUI();
                //Gizmos.DrawIcon((start + (end - start) * edge.RequiredUntil + offset), "Required Edge.png", true);
            }
        }
    }

}
