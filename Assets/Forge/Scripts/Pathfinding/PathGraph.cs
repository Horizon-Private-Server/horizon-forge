using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Unity.VisualScripting;
using UnityEditor;
using UnityEditor.Experimental.GraphView;
using UnityEngine;
using UnityEngine.UIElements;

public class PathGraph : MonoBehaviour
{
    [SerializeField, HideInInspector]
    private List<PathGraphNode> _cachedNodes;

    [Min(1)] public int MaxExportedPathLength = 3;
    public List<PathGraphEdge> Edges = new List<PathGraphEdge>();

    public void RefreshCache()
    {
        _cachedNodes = GetNodes();
    }

    public int? GetNodeCount()
    {
        return _cachedNodes?.Count;
    }

    public bool CanSeeNode(PathGraphNode node)
    {
        if (_cachedNodes == null)
            _cachedNodes = GetNodes();

        if (!_cachedNodes.Any())
            return true;

        var nodeCenterPoint = node.GetCenterPosition();
        foreach (var otherNode in _cachedNodes)
        {
            var otherNodeCenterPoint = otherNode.GetCenterPosition();
            var delta = otherNodeCenterPoint - nodeCenterPoint;
            if (!Physics.Raycast(nodeCenterPoint, delta.normalized, delta.magnitude) && !Physics.Raycast(otherNodeCenterPoint, -delta.normalized, delta.magnitude))
            {
                return true;
            }
        }

        return false;
    }

    public bool CanSeeNode(PathGraphNode a, PathGraphNode b)
    {
        if (_cachedNodes == null)
            _cachedNodes = GetNodes();

        if (!_cachedNodes.Any())
            return true;

        var aCenterPoint = a.GetCenterPosition();
        var bCenterPoint = b.GetCenterPosition();
        var delta = bCenterPoint - aCenterPoint;
        if (!Physics.Raycast(aCenterPoint, delta.normalized, delta.magnitude) && !Physics.Raycast(bCenterPoint, -delta.normalized, delta.magnitude))
        {
            return true;
        }

        return false;
    }

    public bool CanSeeNode(PathGraphNode a, Vector3 b)
    {
        if (_cachedNodes == null)
            _cachedNodes = GetNodes();

        if (!_cachedNodes.Any())
            return true;

        var aCenterPoint = a.GetCenterPosition();
        var delta = b - aCenterPoint;
        if (!Physics.Raycast(aCenterPoint, delta.normalized, delta.magnitude) && !Physics.Raycast(b, -delta.normalized, delta.magnitude))
        {
            return true;
        }

        return false;
    }

    public int GetNodeIndex(PathGraphNode node)
    {
        if (_cachedNodes == null)
            _cachedNodes = GetNodes();

        return _cachedNodes.IndexOf(node);
    }

    public List<PathGraphNode> GetNodes()
    {
        return GetComponentsInChildren<PathGraphNode>()?.ToList() ?? new List<PathGraphNode>();
    }

    private void OnDrawGizmosSelected()
    {
        //DrawBounds(GetBounds());

        if (_cachedNodes == null)
        {
            RefreshCache();
            return;
        }
    }

    #region A*

    public Stack<PathGraphMemoryNode> FindPath(Vector3 from, Vector3 to, bool cleanup = false)
    {
        var nodes = BuildMemoryGraph();
        var path = new Stack<PathGraphMemoryNode>();

        var start = GetClosestNodeInSight(nodes, from);
        var end = GetClosestNodeInSight(nodes, to);

        Dictionary<PathGraphMemoryNode, float> nodeBestDistance = nodes.ToDictionary(x => x, x => float.MaxValue);
        List<PathGraphMemoryNode> unvisitedNodes = new List<PathGraphMemoryNode>(nodes);

        // set start point best distance to 0
        nodeBestDistance[start] = 0;


        var current = start;
        while (unvisitedNodes.Contains(end)) // && !unvisitedNodes.All(x => nodeBestDistance[x] == float.MaxValue))
        {
            foreach (var edge in current.Edges)
            {
                var neighborNode = edge.ToNode;
                var neighborDist = nodeBestDistance[neighborNode];
                var newDist = nodeBestDistance[current] + edge.Distance;

                if (newDist < neighborDist)
                {
                    nodeBestDistance[neighborNode] = newDist;
                    neighborNode.PathParent = current;
                }
            }

            // mark current as visited
            unvisitedNodes.Remove(current);

            // determine next current as next node with smallest best distance
            current = unvisitedNodes.OrderBy(x => nodeBestDistance[x]).FirstOrDefault();
            if (current == null || nodeBestDistance[current] == float.MaxValue)
                break;
        }

        // build path back from end
        current = end;

        if (cleanup && current.PathParent != null)
        {
            // end
            var dt = to - current.Center;
            if (Vector3.Dot(end.Center - to, current.PathParent.Center - to) < 0) // && !Physics.Raycast(current.Center, dt.normalized, dt.magnitude))
            {
                current = current.PathParent;
            }
        }

        do
        {
            // do some clean up on the first and last nodes
            // so that if its better to just go to the next node from the start position
            // or go directly to the target from the second to last node, then do that
            // this is to counter the problem with the first/last nodes only be "connected" to their closest node
            if (cleanup && current.PathParent == start)
            {
                // start
                if (Vector3.Dot(start.Center - from, current.Center - from) < 0)
                {
                    path.Push(current);
                    break;
                }
            }

            path.Push(current);
            current = current.PathParent;
        }
        while (current != null);

        return path;
    }

    public PathGraphNode GetClosestNode(Vector3 point)
    {
        PathGraphNode closestNode = null;
        float? closestNodeDist = null;

        if (_cachedNodes == null)
            _cachedNodes = GetNodes();

        // get closest node
        // then add that nodes connected nodes
        foreach (var node in _cachedNodes)
        {
            var dist = Mathf.Max(0, Vector3.Distance(node.GetCenterPosition(), point) - node.Radius);

            if (closestNodeDist == null || dist < closestNodeDist)
            {
                closestNode = node;
                closestNodeDist = dist;
            }
        }

        return closestNode;
    }

    public PathGraphNode GetClosestNodeInSight(Vector3 point)
    {
        PathGraphNode closestNode = null;
        float? closestNodeDist = null;

        if (_cachedNodes == null)
            _cachedNodes = GetNodes();

        // get closest node
        // then add that nodes connected nodes
        foreach (var node in _cachedNodes)
        {
            var dist = Mathf.Max(0, Vector3.Distance(node.GetCenterPosition(), point) - node.Radius);
            if (!InSight(node.GetCenterPosition(), point))
                dist *= 1000f;

            if (closestNodeDist == null || dist < closestNodeDist)
            {
                closestNode = node;
                closestNodeDist = dist;
            }
        }

        return closestNode;
    }

    public bool InSight(Vector3 from, Vector3 to)
    {
        var delta = to - from;
        var length = delta.magnitude;

        return !Physics.Raycast(from, delta, length);
    }

    private PathGraphMemoryNode GetClosestNode(List<PathGraphMemoryNode> graph, Vector3 point)
    {
        PathGraphMemoryNode closestNode = null;
        float? closestNodeDist = null;

        // get closest node
        // then add that nodes connected nodes
        foreach (var node in graph)
        {
            var dist = Mathf.Max(0, Vector3.Distance(node.Center, point) - node.Radius);

            if (closestNodeDist == null || dist < closestNodeDist)
            {
                closestNode = node;
                closestNodeDist = dist;
            }
        }

        return closestNode;
    }

    private PathGraphMemoryNode GetClosestNodeInSight(List<PathGraphMemoryNode> graph, Vector3 point)
    {
        PathGraphMemoryNode closestNode = null;
        float? closestNodeDist = null;

        // get closest node
        // then add that nodes connected nodes
        foreach (var node in graph)
        {
            var dist = Mathf.Max(0, Vector3.Distance(node.Center, point) - node.Radius);
            if (Physics.Raycast(node.Center, point - node.Center, Vector3.Distance(node.Center, point), LayerMask.GetMask("LEVEL")))
                dist *= 1000f;

            if (closestNodeDist == null || dist < closestNodeDist)
            {
                closestNode = node;
                closestNodeDist = dist;
            }
        }

        return closestNode;
    }

    private List<PathGraphMemoryNode> BuildMemoryGraph()
    {
        List<PathGraphMemoryNode> nodes = new List<PathGraphMemoryNode>();
        if (_cachedNodes == null)
            _cachedNodes = GetNodes();

        // add nodes
        foreach (var node in _cachedNodes)
        {
            nodes.Add(new PathGraphMemoryNode()
            {
                RefNode = node,
                Center = node.GetCenterPosition(),
                Radius = node.Radius,
                CorneringRadius = node.Cornering * node.Radius,
                Edges = new List<PathGraphMemoryEdge>()
            });
        }

        // add edges
        foreach (var node in nodes)
        {
            if (node.RefNode)
            {
                var edges = Edges.Where(x => x.From == node.RefNode);
                foreach (var edge in edges)
                {
                    var connectedMemNode = nodes.FirstOrDefault(x => x.RefNode == edge.To);
                    if (connectedMemNode != null)
                    {
                        node.Edges.Add(new PathGraphMemoryEdge()
                        {
                            ToNode = connectedMemNode,
                            Distance = Mathf.Max(Vector3.Distance(node.Center, connectedMemNode.Center), 0) * edge.CostFactor,
                            PathFitStartEnd = edge.PathFitStartEnd,
                            JumpPad = edge.JumpPad
                        });
                    }
                }
            }
        }

        return nodes;
    }

    #endregion


    #region Export

    public static string ExportGraphsAsC()
    {
        int i = 0;
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return "";

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

        return sb.ToString() + sbPathGraphDefs.ToString();
    }

    public void ExportAsC(string varPrefix, out string dataDefs, out string pathGraphDef)
    {
        dataDefs = "";

        string nodesVarName = $"{varPrefix}_PATHFINDING_NODES";
        string nodesCorneringVarName = $"{varPrefix}_PATHFINDING_NODES_CORNERING";
        string nodesHeightVarName = $"{varPrefix}_PATHFINDING_NODES_HEIGHT";
        string edgesVarName = $"{varPrefix}_PATHFINDING_EDGES";
        string edgesRequiredVarName = $"{varPrefix}_PATHFINDING_EDGES_REQUIRED";
        string edgesPathFitVarName = $"{varPrefix}_PATHFINDING_EDGES_PATHFIT";
        string edgesJumpPadSpeedVarName = $"{varPrefix}_PATHFINDING_EDGES_JUMPPADSPEED";
        string edgesJumpPadAtVarName = $"{varPrefix}_PATHFINDING_EDGES_JUMPPADAT";

        _cachedNodes = GetNodes();

        // max # of nodes
        if (_cachedNodes.Count > 100)
        {
            Debug.LogError($"{this.name} num nodes {_cachedNodes.Count} exceeds max 100");
        }

        // build list of nodes
        dataDefs += $"VECTOR {nodesVarName}[] = {{\n";
        foreach (var node in _cachedNodes)
        {
            var p = node.GetCenterPosition();
            dataDefs += $"\t{{ {p.x}, {p.z}, {p.y}, {node.Radius} }},\n";
        }
        dataDefs += "};\n\n";

        // build list of node cornering amount
        dataDefs += $"u8 {nodesCorneringVarName}[] = {{\n";
        foreach (var node in _cachedNodes)
        {
            dataDefs += $"\t{(int)(node.Cornering * 255)},\n";
        }
        dataDefs += "};\n\n";

        // build list of node height amount
        dataDefs += $"u16 {nodesHeightVarName}[] = {{\n";
        foreach (var node in _cachedNodes)
        {
            var heightLimit = node.HasHeightLimit ? (node.HeightLimit * 256) : ushort.MaxValue;
            dataDefs += $"\t{(ushort)(Mathf.Clamp(heightLimit, 0, ushort.MaxValue))},\n";
        }
        dataDefs += "};\n\n";

        // build list of edges
        dataDefs += $"u8 {edgesVarName}[][2] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{{ {_cachedNodes.IndexOf(edge.From)}, {_cachedNodes.IndexOf(edge.To)} }},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge requireds
        dataDefs += $"u8 {edgesRequiredVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(edge.Required ? (int)(Mathf.Clamp(edge.RequiredUntil * 255 + 1, 0, 255)) : 0)},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge path fit start end
        dataDefs += $"u8 {edgesPathFitVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(int)(edge.PathFitStartEnd * 255)},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge jump pad speeds
        dataDefs += $"u8 {edgesJumpPadSpeedVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(edge.JumpPad ? (int)Math.Ceiling(edge.JumpPadSpeed + 0.01) : 0)},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge jump pad ats
        dataDefs += $"u16 {edgesJumpPadAtVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(edge.JumpPad ? (int)(edge.JumpPadAt * ushort.MaxValue) : 0)},\n";
        }
        dataDefs += "};\n\n";
        dataDefs += ExportPathsAsC(varPrefix, out int longestPath);

        pathGraphDef = @$"  {{
    .NumNodes = {_cachedNodes.Count},
    .NumEdges = {Edges.Count},
    .MaxPathNodeCount = {longestPath},
    .Nodes = {nodesVarName},
    .Cornering = {nodesCorneringVarName},
    .Heights = {nodesHeightVarName},
    .Edges = {edgesVarName},
    .EdgesRequired = {edgesRequiredVarName},
    .EdgesPathFit = {edgesPathFitVarName},
    .EdgesJumpSpeed = {edgesJumpPadSpeedVarName},
    .EdgesJumpAt = {edgesJumpPadAtVarName},
    .Paths = (u8 *){varPrefix}_PATHFINDING_PATHS,
    .LastTargetUpdatedIdx = -1
  }},";
    }

    public string ExportAsSurvivalC()
    {
        var dataDefs = "";
        var varPrefix = "MOB";

        string nodesVarName = $"{varPrefix}_PATHFINDING_NODES";
        string nodesCorneringVarName = $"{varPrefix}_PATHFINDING_NODES_CORNERING";
        string nodesHeightVarName = $"{varPrefix}_PATHFINDING_NODES_HEIGHT";
        string edgesVarName = $"{varPrefix}_PATHFINDING_EDGES";
        string edgesRequiredVarName = $"{varPrefix}_PATHFINDING_EDGES_REQUIRED";
        string edgesPathFitVarName = $"{varPrefix}_PATHFINDING_EDGES_PATHFIT";
        string edgesJumpPadSpeedVarName = $"{varPrefix}_PATHFINDING_EDGES_JUMPPADSPEED";
        string edgesJumpPadAtVarName = $"{varPrefix}_PATHFINDING_EDGES_JUMPPADAT";

        _cachedNodes = GetNodes();

        // max # of nodes
        if (_cachedNodes.Count > 100)
        {
            Debug.LogError($"{this.name} num nodes {_cachedNodes.Count} exceeds max 100");
        }

        // build list of nodes
        dataDefs += $"VECTOR {nodesVarName}[] = {{\n";
        foreach (var node in _cachedNodes)
        {
            var p = node.GetCenterPosition();
            dataDefs += $"\t{{ {p.x.ToInvariantCulture()}, {p.z.ToInvariantCulture()}, {p.y.ToInvariantCulture()}, {node.Radius.ToInvariantCulture()} }},\n";
        }
        dataDefs += "};\n\n";

        // build list of node cornering amount
        dataDefs += $"u8 {nodesCorneringVarName}[] = {{\n";
        foreach (var node in _cachedNodes)
        {
            dataDefs += $"\t{(int)(node.Cornering * 255)},\n";
        }
        dataDefs += "};\n\n";

        // build list of node height amount
        dataDefs += $"u16 {nodesHeightVarName}[] = {{\n";
        foreach (var node in _cachedNodes)
        {
            var heightLimit = node.HasHeightLimit ? (node.HeightLimit * 256) : ushort.MaxValue;
            dataDefs += $"\t{(ushort)(Mathf.Clamp(heightLimit, 0, ushort.MaxValue))},\n";
        }
        dataDefs += "};\n\n";

        // build list of edges
        dataDefs += $"u8 {edgesVarName}[][2] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{{ {_cachedNodes.IndexOf(edge.From)}, {_cachedNodes.IndexOf(edge.To)} }},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge requireds
        dataDefs += $"u16 {edgesRequiredVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(edge.Required ? (int)(Mathf.Clamp(edge.RequiredUntil * ushort.MaxValue + 1, 0, ushort.MaxValue)) : 0)},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge path fit start end
        dataDefs += $"u8 {edgesPathFitVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(int)(edge.PathFitStartEnd * 255)},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge jump pad speeds
        dataDefs += $"u16 {edgesJumpPadSpeedVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(edge.JumpPad ? (int)Math.Ceiling(edge.JumpPadSpeed + 0.01) : 0)},\n";
        }
        dataDefs += "};\n\n";

        // build list of edge jump pad ats
        dataDefs += $"u16 {edgesJumpPadAtVarName}[] = {{\n";
        foreach (var edge in this.Edges)
        {
            dataDefs += $"\t{(edge.JumpPad ? (int)(edge.JumpPadAt * ushort.MaxValue) : 0)},\n";
        }
        dataDefs += "};\n\n";
        dataDefs += ExportPathsAsC(varPrefix, out int longestPath);

        dataDefs += $"const int MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH = {longestPath};\n";
        dataDefs += $"const int MOB_PATHFINDING_NODES_COUNT = {_cachedNodes.Count};\n";
        dataDefs += $"const int MOB_PATHFINDING_EDGES_COUNT = {Edges.Count};\n";
        return dataDefs;
    }

    public string ExportPathsAsC(string varPrefix, out int longestPath)
    {
        var str = "";
        var edgeIdxs = new List<int>();

        _cachedNodes = GetNodes();

        longestPath = 0;

        var paths = new List<List<int>>();
        foreach (var node1 in _cachedNodes)
        {
            foreach (var node2 in _cachedNodes)
            {
                edgeIdxs = new List<int>();

                if (node1 != node2)
                {
                    var path = this.FindPath(node1.GetCenterPosition(), node2.GetCenterPosition(), cleanup: false).ToList();

                    var lastNode = path[0]?.RefNode;
                    foreach (var n in path.Skip(1))
                    {
                        var edge = this.Edges.FirstOrDefault(x => x.From == lastNode && x.To == n.RefNode);
                        if (edge == null)
                            break;

                        lastNode = n.RefNode;
                        edgeIdxs.Add(this.Edges.IndexOf(edge));
                    }

                    if (edgeIdxs.Count > longestPath)
                        longestPath = edgeIdxs.Count;
                }

                paths.Add(edgeIdxs);
            }
        }

        // limit longest path
        if (longestPath > MaxExportedPathLength)
            longestPath = MaxExportedPathLength;

        // count unique paths
        //var uniquePaths = new HashSet<string>();
        //foreach (var path in paths)
        //{
        //    var pathStr = "";
        //    for (int i = 0; i < longestPath; ++i)
        //    {
        //        pathStr += "," + path.ElementAtOrDefault(i);
        //    }
        //    uniquePaths.Add(pathStr);
        //}
        //Debug.Log($"Found {uniquePaths.Count}/{paths.Count} unique paths for {gameObject.name}");
        
        // build list of nodes
        str += $"u8 {varPrefix}_PATHFINDING_PATHS[][{longestPath}] = {{\n";
        foreach (var path in paths)
        {
            str += "\t{ ";
            for (int i = 0; i < longestPath; ++i)
            {
                if (i < path.Count)
                    str += $"{path[i]}, ";
                else
                    str += $"{255}, ";
            }

            str += "},\n";
        }
        str += "};\n\n";

        //Debug.Log($"longest path {longestPath}");

        return str;
    }

    #endregion
}

[Serializable]
public class PathGraphEdge
{
    public PathGraphNode From;
    public PathGraphNode To;
    public float CostFactor = 1f;
    [Range(0f, 1f)] public float PathFitStartEnd = 0.5f;
    public bool Required = false;
    [Range(0f, 1f)] public float RequiredUntil = 0f;
    public bool JumpPad = false;
    public float JumpPadSpeed = 0f;
    [Range(0f, 1f)] public float JumpPadAt = 0f;
}

public class PathGraphMemoryNode
{
    public Vector3 Center;
    public float Radius;
    public float CorneringRadius;
    public PathGraphNode RefNode;

    public PathGraphMemoryNode PathParent;

    public List<PathGraphMemoryEdge> Edges;
}

public class PathGraphMemoryEdge
{
    public PathGraphMemoryNode ToNode;
    public float Distance;
    public float PathFitStartEnd;
    public bool JumpPad;
}

public class PriorityQueue<TElement, TPriority>
{
    private List<Item> _collection = new List<Item>();

    public struct Item { public TElement Element; public TPriority Priority; }

    public List<Item> UnorderedItems => _collection;
    public int Count => _collection.Count;

    public void Enqueue(TElement element, TPriority priority)
    {
        _collection.Add(new Item() { Element = element, Priority = priority });
    }

    public TElement Dequeue()
    {
        var highestPriority = _collection.OrderByDescending(x => x.Priority).FirstOrDefault();
        _collection.Remove(highestPriority);
        return highestPriority.Element;
    }
}
