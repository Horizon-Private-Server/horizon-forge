using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class Spline : MonoBehaviour
{
    public static bool DrawSplineGizmos = false;
    public static List<Spline> SelectedSplines = new List<Spline>();

    public List<SplineVertex> Vertices;

    protected virtual void Start()
    {
        RefreshVertices();
    }

    protected virtual void OnValidate()
    {
        RefreshVertices();
    }

    public virtual void RefreshVertices()
    {
        Vertices = GetComponentsInChildren<SplineVertex>().ToList();
    }

    public static void UpdateDrawGizmos()
    {
        DrawSplineGizmos = false;
        SelectedSplines.Clear();
        if (!Selection.activeGameObject) return;

        if (Selection.activeGameObject.GetComponent<Spline>() is Spline spline)
        {
            SelectedSplines.Add(spline);
            DrawSplineGizmos = true;
            return;
        }

        if (Selection.activeGameObject.GetComponent<SplineVertex>() is SplineVertex splineVertex)
        {
            SelectedSplines.Add(splineVertex.GetComponentInParent<Spline>());
            DrawSplineGizmos = true;
            return;
        }

        if (Selection.activeGameObject.GetComponent<Area>() is Area area && area.Splines != null)
        {
            SelectedSplines.AddRange(area.Splines);
            DrawSplineGizmos = area.Splines.Any(x => x); // area has a spline
            return;
        }

        if (Selection.activeGameObject.GetComponent<Moby>() is Moby moby)
        {
            var mobySplines = moby.PVarReferences.Select(x => x.Value as Spline).Where(x => x);
            var mobyAreas = moby.PVarReferences.Select(x => x.Value as Area).Where(x => x);
            SelectedSplines.AddRange(mobySplines);
            DrawSplineGizmos = mobySplines.Any() || mobyAreas.Any(x => x.Splines.Any());
            return;
        }
    }

    protected virtual void OnDrawGizmos()
    {
        if (DrawSplineGizmos)
        {
            Gizmos.color = SelectedSplines.Contains(this) ? Color.red : Color.white;
            DrawGizmos();
        }
    }

    protected virtual void DrawGizmos()
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

    public virtual List<Vector3> ReadSpline(BinaryReader reader)
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

    public virtual void Write(BinaryWriter writer)
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
