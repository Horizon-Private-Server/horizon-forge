using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class BezierSpline : Spline
{
    public enum BezierSplineGenMode
    {
        FixedCount,
        Curvature
    }

    [Header("Spline")]
    public BezierSplineGenMode Mode = BezierSplineGenMode.FixedCount;
    [Min(2)]
    public int NumPoints = 2;

    [Range(0f, 0.99f)]
    public float Curvature = 0f;
    public bool Loop = false;

    [ReadOnly] public int ComputedNumPoints = 0;

    [Header("Gizmos")]
    public bool DrawPoints = false;
    public bool DrawRotation = false;

    private BezierSplineVertex[] _cachedVertices;

    public BezierSplineVertex[] GetVertices()
    {
        if (_cachedVertices != null && _cachedVertices.All(x => x))
            return _cachedVertices;

        _cachedVertices = GetComponentsInChildren<BezierSplineVertex>();
        if (Loop && _cachedVertices != null)
        {
            Array.Resize(ref _cachedVertices, _cachedVertices.Length + 1);
            _cachedVertices[_cachedVertices.Length - 1] = _cachedVertices[0];
        }

        return _cachedVertices;
    }

    protected override void Start()
    {
        InvalidateCache();
    }

    protected override void OnValidate()
    {
        InvalidateCache();
        BuildSpline();
    }

    #region Create Asset


    [MenuItem("GameObject/Forge/Misc/Bezier Spline", priority = 10)]
    public static void CreateBezierSpline()
    {
        var go = new GameObject("Bezier Spline");
        var bezier = go.AddComponent<BezierSpline>();
        var vertGo0 = new GameObject("0");
        var vertGo1 = new GameObject("1");

        vertGo0.transform.SetParent(go.transform, false);
        vertGo1.transform.SetParent(go.transform, false);
        vertGo1.transform.localPosition = Vector3.right * 10;

        var vert0 = vertGo0.AddComponent<BezierSplineVertex>();
        var vert1 = vertGo1.AddComponent<BezierSplineVertex>();

        vert0.SetOffsets(Vector3.forward * -5, Vector3.forward * 5);
        vert1.SetOffsets(Vector3.forward * -5, Vector3.forward * 5);

        bezier.Mode = BezierSplineGenMode.Curvature;
        bezier.NumPoints = 20;
        bezier.Curvature = 0.9f;

        // place under selected object
        // or try and spawn on top of scene camera
        if (Selection.activeGameObject)
            go.transform.SetParent(Selection.activeGameObject.transform, false);
        else if (SceneView.lastActiveSceneView.camera)
            go.transform.position = SceneView.lastActiveSceneView.camera.transform.position + (SceneView.lastActiveSceneView.camera.transform.forward * 5);

        Selection.activeGameObject = go;
    }

    #endregion

    #region Cache

    public void InvalidateCache()
    {
        _cachedVertices = null;
    }

    #endregion

    #region Spline

    public override void RefreshVertices()
    {
        Vertices = GetComponentsInChildren<SplineVertex>().ToList();
        //BuildSpline();
    }

    private void BuildSpline()
    {
        // remove existing -- destroying SplineVertex calls RefreshVertices()
        var vertices = GetComponentsInChildren<SplineVertex>();
        Dispatcher.RunOnMainThread(() => {
            foreach (var vertex in vertices)
                if (vertex)
                    GameObject.DestroyImmediate(vertex.gameObject);
        });

        // build path
        var path = ComputePath();
        if (path == null)
            return;

        // add vertices
        Vertices.Clear();
        for (int i = 0; i < (path.Length - 1); ++i)
        {
            var pos = path[i];
            var pos2 = path[i + 1];
            var go = new GameObject(i.ToString());
            var vertex = go.AddComponent<SplineVertex>();
            go.transform.SetParent(this.transform.transform, false);

            go.transform.position = pos;
            go.transform.rotation = Quaternion.LookRotation((pos2 - pos).normalized, Vector3.up);
            go.hideFlags = HideFlags.HideAndDontSave;

            Vertices.Add(vertex);
        }
    }

    public override void Write(BinaryWriter writer)
    {
        BuildSpline();

        base.Write(writer);
    }

    #endregion

    #region Bezier

    public BezierSplineNear GetNearestPoint(Vector3 position)
    {
        const float step = 0.01f;
        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2) return new BezierSplineNear();

        Vector3 nearest = vertices[0].Control;
        BezierSplineVertex nearestPoint = vertices[0];
        float nearestTime = 0f;
        float nearestDist = float.MaxValue;

        for (int i = 0; i < (vertices.Length - 1); ++i)
        {
            for (float t = 0f; t <= 1f; t += step)
            {
                var p = GetPosition(vertices[i], vertices[i + 1], t);
                var d = Vector3.Distance(position, p);
                if (d < nearestDist)
                {
                    nearest = p;
                    nearestPoint = vertices[i];
                    nearestTime = t;
                    nearestDist = d;
                }
            }
        }

        return new BezierSplineNear()
        {
            Vertex = nearestPoint,
            Position = nearest,
            Time = nearestTime
        };
    }

    public float GetDistanceToPointOnCurve(BezierSplineVertex vertex, float time)
    {
        float distance = 0f;
        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2) return 0;

        for (int i = 0; i < (vertices.Length - 1); ++i)
        {
            var p = vertices[i];
            if (p == vertex)
                return distance += GetSegmentLength(p, vertices[i + 1], 0, time);

            distance += GetSegmentLength(p, vertices[i + 1], 0, 1f);
        }

        return distance;
    }

    public (BezierSplineVertex a, BezierSplineVertex b, float time) GetPointOnPath(float distance)
    {
        float currentDistance = 0f;
        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2) return (null, null, 0);

        for (int i = 0; i < vertices.Length - 1; ++i)
        {
            // find segment
            float segmentLength = GetSegmentLength(vertices[i], vertices[i + 1], 0, 1);
            if ((currentDistance + segmentLength) < distance)
            {
                currentDistance += segmentLength;
                continue;
            }

            // find time
            Vector3 lastPosition = GetPosition(vertices[i], vertices[i + 1], 0);
            for (float t = 0f; t <= 1f; t += 0.001f)
            {
                var p = GetPosition(vertices[i], vertices[i + 1], t);
                var d = Vector3.Distance(lastPosition, p);
                currentDistance += d;
                if (currentDistance >= distance)
                    return (vertices[i], vertices[i + 1], t);

                lastPosition = p;
            }
        }

        return (vertices[vertices.Length - 2], vertices[vertices.Length - 1], 1);
    }

    public float GetTimeToDistanceOnPoint(BezierSplineVertex a, BezierSplineVertex b, float distance)
    {
        float t = 0f;
        float currentDistance = 0f;

        // find time
        Vector3 lastPosition = a.Control;
        for (t = 0f; t <= 1f; t += 0.001f)
        {
            var p = GetPosition(a, b, t);
            var d = Vector3.Distance(lastPosition, p);
            currentDistance += d;
            if (currentDistance >= distance)
                return t;

            lastPosition = p;
        }

        return t;
    }

    public float GetSegmentLength(BezierSplineVertex a, BezierSplineVertex b, float tStart, float tEnd)
    {
        const float step = 0.01f;
        float distance = 0f;

        var lastPos = GetPosition(a, b, tStart);

        for (float t = tStart + step; t <= tEnd; t += step)
        {
            var curPos = GetPosition(a, b, t);
            distance += Vector3.Distance(curPos, lastPos);
            lastPos = curPos;
        }

        return distance;
    }

    public Vector3[] ComputePath()
    {
        List<(BezierSplineVertex a, BezierSplineVertex b, float time)> pointsT = new List<(BezierSplineVertex a, BezierSplineVertex b, float time)>();

        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2)
            return null;

        // compute length of curve
        var length = 0f;
        for (int i = 0; i < (vertices.Length - 1); ++i)
            length += GetSegmentLength(vertices[i], vertices[i + 1], 0, 1);

        // iterate by length
        if (Mode == BezierSplineGenMode.Curvature)
        {
            for (int i = 0; i < (vertices.Length - 1); ++i)
            {
                var a = vertices[i];
                var b = vertices[i + 1];
                var t = 0f;
                var step = 1 / 10f;
                while (t < 1)
                {
                    var point = (a, b, t);
                    var pos = GetPosition(point.a, point.b, point.t);

                    pointsT.Add(point);
                    t += step;
                }
            }
        }
        else
        {
            var lengthStep = length / (NumPoints - 1);
            var currentLength = 0f;
            while (currentLength < length)
            {
                var point = GetPointOnPath(currentLength);
                var pos = GetPosition(point.a, point.b, point.time);

                pointsT.Add(point);
                currentLength += lengthStep;
            }
        }

        // add end
        pointsT.Add((vertices[vertices.Length - 2], vertices[vertices.Length - 1], 1));

        // increase density of points around curves
        if (Mode == BezierSplineGenMode.Curvature)
        {
            var idx = 1;
            while (idx < (pointsT.Count - 1))
            {
                // before
                var currentIdx = idx;
                {
                    // 0 = 180 deg turn
                    // 1 = 0 deg turn (straight line)
                    var p0 = pointsT[currentIdx - 1];
                    var p1 = pointsT[currentIdx];
                    var p2 = pointsT[currentIdx + 1];
                    var t01 = GetTangent(p1.a, p1.b, p1.time);
                    var t12 = GetTangent(p2.a, p2.b, p2.time);
                    var factor = (Vector3.Dot(t01, t12) + 1) / 2;

                    while (factor < Curvature && p1.a == p0.a)
                    {
                        var t = Mathf.Lerp(p0.time, p1.time, 0.5f);
                        pointsT.Insert(currentIdx + 0, (p1.a, p1.b, t));
                        idx += 1;

                        p0 = pointsT[currentIdx - 1];
                        p1 = pointsT[currentIdx];
                        p2 = pointsT[currentIdx + 1];

                        t01 = GetTangent(p0.a, p0.b, p0.time);
                        t12 = GetTangent(p1.a, p1.b, p1.time);
                        factor = (Vector3.Dot(t01, t12) + 1) / 2;

                        if ((idx - currentIdx) > 100) break;
                    }
                }

                // after
                currentIdx = idx + 0;
                {
                    // 0 = 180 deg turn
                    // 1 = 0 deg turn (straight line)
                    var p0 = pointsT[currentIdx - 1];
                    var p1 = pointsT[currentIdx];
                    var p2 = pointsT[currentIdx + 1];
                    var t01 = GetTangent(p1.a, p1.b, p1.time);
                    var t12 = GetTangent(p2.a, p2.b, p2.time);
                    var factor = (Vector3.Dot(t01, t12) + 1) / 2;

                    while (factor < Curvature && p1.a == p2.a)
                    {
                        var t = Mathf.Lerp(p1.time, p2.time, 0.5f);
                        pointsT.Insert(currentIdx + 1, (p1.a, p1.b, t));
                        idx += 1;

                        p0 = pointsT[currentIdx - 1];
                        p1 = pointsT[currentIdx];
                        p2 = pointsT[currentIdx + 1];

                        t01 = GetTangent(p1.a, p1.b, p1.time);
                        t12 = GetTangent(p2.a, p2.b, p2.time);
                        factor = (Vector3.Dot(t01, t12) + 1) / 2;

                        if ((idx - currentIdx) > 100) break;
                    }

                    ++currentIdx;
                }

                ++idx;
            }
        }

        // build points
        Vector3[] points = new Vector3[pointsT.Count];
        for (int i = 0; i < pointsT.Count; ++i)
        {
            points[i] = GetPosition(pointsT[i].a, pointsT[i].b, pointsT[i].time);
        }

        return points;
    }

    private Vector3 GetPosition(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        float invT = 1 - t;
        return (a.Control * Mathf.Pow(invT, 3)) +
            (a.HandleOut * 3 * t * Mathf.Pow(invT, 2)) +
            (b.HandleIn * 3 * Mathf.Pow(t, 2) * invT) +
            (b.Control * Mathf.Pow(t, 3));
    }

    private Vector3 GetTangent(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        float delta = 0.001f;
        if (t >= 1f)
        {
            return ((GetPosition(a, b, t) - GetPosition(a, b, t - delta)) / delta).normalized;
        }
        else
        {
            return ((GetPosition(a, b, t + delta) - GetPosition(a, b, t)) / delta).normalized;
        }
    }

    private Vector3 GetNormal(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        var tan = GetTangent(a, b, t);
        var normal = Vector3.Cross(tan, Vector3.right);
        if (normal.sqrMagnitude == 0f)
            return Vector3.Cross(tan, Vector3.up);

        return normal;
    }

    #endregion

    #region Gizmos

    private void OnDrawGizmos()
    {
        if (!IsSplineSelected()) return;

        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2)
            return;

        // compute length of curve
        var length = 0f;
        for (int i = 0; i < (vertices.Length - 1); ++i)
            length += GetSegmentLength(vertices[i], vertices[i + 1], 0, 1);

        var path = ComputePath();
        if (path == null) return;

        for (int i = 0; i < (path.Length-1); ++i)
        {
            var pos = path[i];
            var pos2 = path[i + 1];
            var tan = (pos2 - pos).normalized;
            var normal = Vector3.up; // GetNormal(Points[i], Points[i + 1], t);
            var bitangent = Vector3.Cross(tan, normal);
            var rot = Quaternion.LookRotation(tan, normal);
            normal = -Vector3.Cross(tan, bitangent);

            Gizmos.color = Color.white;
            Gizmos.DrawLine(pos, pos2);

            if (DrawPoints)
            {
                Gizmos.DrawSphere(pos, 0.3f);
            }

            // draw rotation
            if (DrawRotation)
            {
                Gizmos.color = Color.blue;
                Gizmos.DrawLine(pos, pos + tan);
                Gizmos.color = Color.green;
                Gizmos.DrawLine(pos, pos + normal);
                Gizmos.color = Color.red;
                Gizmos.DrawLine(pos, pos + bitangent);
            }
        }

        ComputedNumPoints = path.Length;
    }

    private void DrawLineGizmos(BezierSplineVertex a, BezierSplineVertex b, float time, Vector3 drawFrom)
    {
        // draw
        var pos = GetPosition(a, b, time);
        var tan = GetTangent(a, b, time);
        var normal = Vector3.up; // GetNormal(Points[i], Points[i + 1], t);
        var bitangent = Vector3.Cross(tan, normal);
        var rot = Quaternion.LookRotation(tan, normal);

        Gizmos.DrawLine(drawFrom, pos);

        // draw rotation
        if (DrawRotation)
        {
            Gizmos.color = Color.blue;
            Gizmos.DrawLine(pos, pos + tan);
            Gizmos.color = Color.green;
            Gizmos.DrawLine(pos, pos + normal);
            Gizmos.color = Color.red;
            Gizmos.DrawLine(pos, pos + bitangent);
        }
    }

    public bool IsSplineSelected()
    {
        // this is selected
        if (Selection.activeGameObject == this.gameObject) return true;

        var vertices = GetVertices();
        if (vertices == null) return false;

        // any child vertex is selected
        if (vertices.Any(v => v.gameObject == Selection.activeGameObject))
            return true;

        return false;
    }

    #endregion

}

public class BezierSplineNear
{
    public BezierSplineVertex Vertex { get; set; }
    public float Time { get; set; }
    public Vector3 Position { get; set; }
}
