using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public class OcclusionGraph : MonoBehaviour
{
    static readonly Vector3[] corners = new Vector3[]
    {
        new Vector3(1f, 1f, 1f) * 2,
        new Vector3(1f, 1f, -1f) * 2,
        new Vector3(1f, -1f, 1f) * 2,
        new Vector3(1f, -1f, -1f) * 2,
        new Vector3(-1f, 1f, 1f) * 2,
        new Vector3(-1f, 1f, -1f) * 2,
        new Vector3(-1f, -1f, 1f) * 2,
        new Vector3(-1f, -1f, -1f) * 2,
    };

    [SerializeField]
    public List<OcclusionNode> _nodes;

    public void RefreshCache()
    {

    }

    public void ValidateCache()
    {

    }

    public void RegisterNode(OcclusionNode node)
    {
        if (node.transform.root != this.transform)
            node.transform.SetParent(this.transform, true);

        if (_nodes == null) _nodes = new List<OcclusionNode>();
        if (!_nodes.Contains(node))
            _nodes.Add(node);
    }

    public void UnregisterNode(OcclusionNode node)
    {
        // remove from network
        _nodes.Remove(node);
    }

    private void OnValidate()
    {
        ValidateCache();
    }

    private void OnDrawGizmosSelected()
    {

    }

    private void OnDrawGizmos()
    {
        if (Selection.activeGameObject && (Selection.activeGameObject.GetComponent<OcclusionVolume>() || Selection.activeGameObject.GetComponent<OcclusionNode>() || Selection.activeGameObject.GetComponent<OcclusionGraph>()))
        {
            DrawGizmos();
        }
    }

    void DrawGizmos()
    {
        if (_nodes != null)
        {
            foreach (var node in _nodes)
            {
                Gizmos.DrawWireSphere(node.transform.position, 1f);

                //foreach (var b in node.ConnectedNodes)
                //    Gizmos.DrawLine(node.transform.position, b.transform.position);
            }
        }
    }


    public bool HasNode(Vector3 octant)
    {
        foreach (var node in _nodes)
        {
            foreach (var corner in corners)
            {
                var pos = octant + corner;
                var delta = node.transform.position - pos;
                if (!Physics.Raycast(pos, delta.normalized, delta.magnitude) && !Physics.Raycast(node.transform.position, -delta.normalized, delta.magnitude))
                {
                    return true;
                }
            }
        }

        return false;
    }


    public bool CanSeeAnyNode(Vector3 point)
    {
        foreach (var node in _nodes)
        {
            var delta = node.transform.position - point;
            if (!Physics.Raycast(point, delta.normalized, delta.magnitude) && !Physics.Raycast(node.transform.position, -delta.normalized, delta.magnitude))
            {
                return true;
            }
        }

        return false;
    }
}
