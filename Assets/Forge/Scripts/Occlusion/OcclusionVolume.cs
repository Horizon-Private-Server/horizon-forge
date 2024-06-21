using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class OcclusionVolume : MonoBehaviour
{
    [SerializeField, HideInInspector]
    private List<Vector3> _cachedOctants;

    [SerializeField, HideInInspector]
    public Matrix4x4 LastTRS;

    [SerializeField, HideInInspector]
    public bool LastSticky;

    [SerializeField, HideInInspector]
    public float LastStickyDistance;

    public bool Negate;
    public bool ForceAdd;
    public bool Sticky;
    public float StickyDistance = 12;

    public void Align()
    {
        this.transform.rotation = Quaternion.identity;
        this.transform.position = new Vector3((int)(this.transform.position.x / 4.0f) * 4, (int)(this.transform.position.y / 4.0f) * 4, (int)(this.transform.position.z / 4.0f) * 4);
        this.transform.localScale = new Vector3((int)(this.transform.localScale.x / 4.0f) * 4, (int)(this.transform.localScale.y / 4.0f) * 4, (int)(this.transform.localScale.z / 4.0f) * 4);
    }

    public void RefreshCache()
    {
        LastTRS = transform.localToWorldMatrix;
        LastSticky = Sticky;
        LastStickyDistance = StickyDistance;
        _cachedOctants = GetOctants();
    }

    public void ValidateCache()
    {
        if (transform.localToWorldMatrix != LastTRS || Sticky != LastSticky || LastStickyDistance != StickyDistance)
        {
            _cachedOctants = null;
        }
    }

    private void OnDrawGizmosSelected()
    {
        //DrawBounds(GetBounds());

        if (_cachedOctants == null)
        {
            RefreshCache();
            return;
        }

        Gizmos.color = Negate ? Color.red : Color.green;
        foreach (var octant in _cachedOctants)
            Gizmos.DrawWireCube(octant + Vector3.one * 2f, Vector3.one * 0.5f);
    }

    private void OnDrawGizmos()
    {
        if (Selection.activeGameObject && Selection.activeGameObject.GetComponent<OcclusionVolume>())
        {
            DrawGizmos();
        }
    }

    private void DrawGizmos()
    {
        var r = Vector3.right * 0.5f;
        var u = Vector3.up * 0.5f;
        var f = Vector3.forward * 0.5f;

        var m = Gizmos.matrix;
        Gizmos.matrix = this.transform.localToWorldMatrix;
        Gizmos.DrawWireCube(Vector3.zero, Vector3.one);
        Gizmos.DrawLine(r + u + f, -r + u + -f);
        Gizmos.DrawLine(r + u + -f, -r + u + f);
        Gizmos.DrawLine(r + -u + f, -r + -u + -f);
        Gizmos.DrawLine(r + -u + -f, -r + -u + f);

        Gizmos.matrix = m;
    }

    private Vector3 Get(Vector3 corner)
    {
        return this.transform.position + (this.transform.rotation * Vector3.Scale(corner, this.transform.localScale) * 0.5f) - corner * 3.9f;
    }

    public Bounds GetBounds()
    {
        var bounds = new Bounds(transform.position, transform.localScale);

        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(transform.localScale.x, transform.localScale.y, transform.localScale.z) * 0.5f);
        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(transform.localScale.x, transform.localScale.y, -transform.localScale.z) * 0.5f);
        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(transform.localScale.x, -transform.localScale.y, transform.localScale.z) * 0.5f);
        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(transform.localScale.x, -transform.localScale.y, -transform.localScale.z) * 0.5f);
        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(-transform.localScale.x, transform.localScale.y, transform.localScale.z) * 0.5f);
        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(-transform.localScale.x, transform.localScale.y, -transform.localScale.z) * 0.5f);
        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(-transform.localScale.x, -transform.localScale.y, transform.localScale.z) * 0.5f);
        bounds.Encapsulate(transform.position + transform.rotation * new Vector3(-transform.localScale.x, -transform.localScale.y, -transform.localScale.z) * 0.5f);

        return bounds;
    }

    void DrawBounds(Bounds b)
    {
        // bottom
        var p1 = new Vector3(b.min.x, b.min.y, b.min.z);
        var p2 = new Vector3(b.max.x, b.min.y, b.min.z);
        var p3 = new Vector3(b.max.x, b.min.y, b.max.z);
        var p4 = new Vector3(b.min.x, b.min.y, b.max.z);

        Debug.DrawLine(p1, p2, Color.blue);
        Debug.DrawLine(p2, p3, Color.red);
        Debug.DrawLine(p3, p4, Color.yellow);
        Debug.DrawLine(p4, p1, Color.magenta);

        // top
        var p5 = new Vector3(b.min.x, b.max.y, b.min.z);
        var p6 = new Vector3(b.max.x, b.max.y, b.min.z);
        var p7 = new Vector3(b.max.x, b.max.y, b.max.z);
        var p8 = new Vector3(b.min.x, b.max.y, b.max.z);

        Debug.DrawLine(p5, p6, Color.blue);
        Debug.DrawLine(p6, p7, Color.red);
        Debug.DrawLine(p7, p8, Color.yellow);
        Debug.DrawLine(p8, p5, Color.magenta);

        // sides
        Debug.DrawLine(p1, p5, Color.white);
        Debug.DrawLine(p2, p6, Color.gray);
        Debug.DrawLine(p3, p7, Color.green);
        Debug.DrawLine(p4, p8, Color.cyan);
    }

    public int? GetOctantCount()
    {
        return _cachedOctants?.Count;
    }

    public bool Contains(Vector3 point)
    {
        var ls = transform.InverseTransformPoint(point);

        return (ls.x >= -0.5f && ls.x <= 0.5f)
            && (ls.y >= -0.5f && ls.y <= 0.5f)
            && (ls.z >= -0.5f && ls.z <= 0.5f);
    }

    private Vector3 ConstrainToBounds(Vector3 p)
    {
        return p;

        //var ls = transform.InverseTransformPoint(p);
        //var min = -transform.localScale * 0.5f;
        //var max = transform.localScale * 0.5f;

        //if (ls.x < min.x)
        //    ls.x = Mathf.CeilToInt(min.x / 4) * 4;
        //else if (ls.x > max.x)
        //    ls.x = Mathf.FloorToInt(max.x / 4) * 4;

        //if (ls.y < min.y)
        //    ls.y = Mathf.CeilToInt(min.y / 4) * 4;
        //else if (ls.y > max.y)
        //    ls.y = Mathf.FloorToInt(max.y / 4) * 4;

        //if (ls.z < min.z)
        //    ls.z = Mathf.CeilToInt(min.z / 4) * 4;
        //else if (ls.z > max.z)
        //    ls.z = Mathf.FloorToInt(max.z / 4) * 4;

        //var ws = transform.TransformPoint(ls);


        //return ws;
    }

    private Vector3 ToOctant(Vector3 p)
    {
        return new Vector3((int)(p.x / 4) * 4, (int)(p.y / 4) * 4, (int)(p.z / 4) * 4);
    }

    private bool IsNearWalkableSurface(Vector3 p, float? dist = null)
    {
        if (!dist.HasValue)
            dist = 4f * 3f;

        var dirs = new[]
        {
            new Vector3(1f, 1f, 1f),
            new Vector3(1f, 1f, -1f),
            new Vector3(1f, -1f, 1f),
            new Vector3(1f, -1f, -1f),
            new Vector3(-1f, 1f, 1f),
            new Vector3(-1f, 1f, -1f),
            new Vector3(-1f, -1f, 1f),
            new Vector3(-1f, -1f, -1f),
        };

        foreach (var dir in dirs)
        {
            if (Physics.Raycast(p, dir, out var hitInfo, dist.Value))
            {
                if (Vector3.Dot(hitInfo.normal, dir) < 0)
                {
                    //var mr = hitInfo.transform.GetComponent<MeshRenderer>();
                    //if (mr && mr.sharedMaterials.Any(x => x.name.EndsWith("_8")))
                    //    continue;

                    return true;
                }
            }
        }

        return false;
    }

    public List<Vector3> GetOctants()
    {
        var graph = GameObject.FindObjectOfType<OcclusionGraph>();

        var octants = new List<Vector3>();

        var bounds = GetBounds();

        var min = new Vector3((int)(bounds.min.x / 4) * 4, (int)(bounds.min.y / 4) * 4, (int)(bounds.min.z / 4) * 4);
        var max = new Vector3((int)(bounds.max.x / 4) * 4, (int)(bounds.max.y / 4) * 4, (int)(bounds.max.z / 4) * 4);

        var xLen = (int)(transform.localScale.x / 4.0f);
        var yLen = (int)(transform.localScale.y / 4.0f);
        var zLen = (int)(transform.localScale.z / 4.0f);

        for (var x = min.x; x <= max.x; x += 4)
        {
            for (var y = min.y; y <= max.y; y += 4)
            {
                for (var z = min.z; z <= max.z; z += 4)
                {
                    var octant = new Vector3(x, y, z);
                    var center = octant + Vector3.one * 2f;
                    if (Contains(center) && (ForceAdd || !graph || graph.HasNode(center)))
                    {
                        if (Sticky)
                        {
                            if (IsNearWalkableSurface(center, StickyDistance))
                                octants.Add(octant);
                        }
                        else
                        {
                            octants.Add(octant);
                        }
                    }
                }
            }
        }

        return octants;
    }
}
