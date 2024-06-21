using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class Spline : MonoBehaviour
{
    public List<SplineVertex> Vertices;

    private void Start()
    {
        RefreshVertices();
    }

    private void OnValidate()
    {
        RefreshVertices();
    }

    public void RefreshVertices()
    {
        Vertices = GetComponentsInChildren<SplineVertex>().ToList();
    }

    public bool ShouldDrawGizmos(out bool selected)
    {
        selected = false;
        if (!Selection.activeGameObject) return false;

        if (Selection.activeGameObject.GetComponent<Spline>() is Spline spline)
        {
            selected = spline == this;
            return true;
        }

        if (Selection.activeGameObject.GetComponent<SplineVertex>() is SplineVertex splineVertex)
        {
            selected = this.Vertices.Contains(splineVertex);
            return true;
        }

        if (Selection.activeGameObject.GetComponent<Area>() is Area area && area.Splines != null)
        {
            selected = area.Splines.Contains(this);
            return area.Splines.Any(x => x); // area has a spline
        }

        if (Selection.activeGameObject.GetComponent<Moby>() is Moby moby)
        {
            selected = (moby.PVarSplineRefs != null && moby.PVarSplineRefs.Contains(this)) || (moby.PVarAreaRefs != null && moby.PVarAreaRefs.Any(a => a && a.Splines != null && a.Splines.Contains(this)));
            return (moby.PVarSplineRefs != null && moby.PVarSplineRefs.Any(x => x)) || (moby.PVarAreaRefs != null && moby.PVarAreaRefs.Any(x => x && x.Splines != null && x.Splines.Any(s => s))); // has a spline or area w/ spline
        }

        return false;
    }

    private void OnDrawGizmos()
    {
        if (ShouldDrawGizmos(out var selected))
        {
            Gizmos.color = selected ? Color.red : Color.white;
            DrawGizmos();
        }
    }

    private void DrawGizmos()
    {
        if (Vertices != null)
        {
            if (Vertices.Count > 1)
            {
                for (int i = 0; i < (Vertices.Count - 1); ++i)
                {
                    Gizmos.DrawLine(Vertices[i].transform.position, Vertices[i + 1].transform.position);
                }
            }
        }
    }

    public List<Vector3> ReadSpline(BinaryReader reader)
    {
        var vertices = new List<Vector3>();
        var count = reader.ReadInt32();
        reader.BaseStream.Position += 12;

        for (int i = 0; i < count; ++i)
        {
            vertices.Add(new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle()).SwizzleXZY());
            reader.ReadSingle();
        }

        return vertices;
    }

    public void Write(BinaryWriter writer)
    {
        writer.Write(Vertices.Count);
        writer.Write(0);
        writer.Write(0);
        writer.Write(0);

        for (int i = 0; i < Vertices.Count; ++i)
        {
            var pos = Vertices[i].transform.position;
            writer.Write(pos.x);
            writer.Write(pos.z);
            writer.Write(pos.y);
            writer.Write(0f);
        }
    }
}
