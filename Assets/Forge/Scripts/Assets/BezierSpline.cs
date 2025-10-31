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
        Curvature,
        NoBezier
    }

    [Header("Spline")]
    public BezierSplineGenMode Mode = BezierSplineGenMode.FixedCount;
    [Min(2)]
    public int NumPoints = 2;
    public Vector3 AlignedNormal = Vector3.up;

    [Range(0f, 0.99f)]
    public float Curvature = 0f;
    public bool Loop = false;
    [Tooltip("Use for internal Forge-only tools, where spline isn't needed in game.")]
    public bool DoNotIncludeInBuild = false;

    [ReadOnly] public int ComputedNumPoints = 0;

    [Header("Tristrip")]
    public bool Tristrip;
    public float TristripWidth = 5f;

    [Header("Gizmos")]
    public bool DrawPoints = false;
    public bool DrawRotation = false;

    private BezierSplineVertex[] _cachedVertices;
    private List<(BezierSplineVertex a, BezierSplineVertex b, float time)> _cachedPath;
    private Hash128 _cachedPathHash;

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

    public override bool IncludeInExport()
    {
        return !DoNotIncludeInBuild;
    }

    protected override void Start()
    {
        InvalidateCache();
    }

    protected override void OnValidate()
    {
        Dispatcher.RunOnMainThread(() => RebuildSpline());
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
        _cachedPathHash = default;
        _cachedPath = null;
        _cachedVertices = null;
    }

    private Hash128 ComputeHash()
    {
        var hash = new Hash128();
        var vertices = GetVertices();

        hash = hash.Append(this.transform.localToWorldMatrix);

        // add vertices
        foreach (var vertex in vertices)
        {
            hash = hash.Append(vertex.HandleIn);
            hash = hash.Append(vertex.HandleOut);
            hash = hash.Append(vertex.Control);
        }

        // add spline params
        hash.Append((int)Mode);
        hash.Append(Loop ? 1 : 0);
        hash.Append(Curvature);

        return hash;
    }

    #endregion

    #region Spline

    public void RebuildSpline()
    {
        InvalidateCache();
        BuildSpline();

        var asset = GetComponent<IAsset>();
        if (asset != null && asset.GameObject) asset.UpdateAsset();
    }

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
        Vertices ??= new List<SplineVertex>();
        Vertices.Clear();
        var count = path.Length;
        for (int i = 0; i < count; ++i)
        {
            var nextI = i + 1;
            var pos = path[i];
            var pos2 = nextI >= count ? path[0] : path[nextI];
            if (nextI >= count && !Loop) pos2 = pos;
            var tan = (pos2 - pos).normalized;
            if (tan == Vector3.zero) tan = i > 0 ? (path[i] - path[i - 1]).normalized : this.transform.forward;
            var go = new GameObject(i.ToString());
            var vertex = go.AddComponent<SplineVertex>();
            go.transform.SetParent(this.transform.transform, false);

            go.transform.position = pos;
            go.transform.rotation = Quaternion.LookRotation(tan, Vector3.up);
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

    public List<(BezierSplineVertex a, BezierSplineVertex b, float time)> ComputePathPoints()
    {
        var hash = ComputeHash();
        if (hash == _cachedPathHash && _cachedPath != null)
            return _cachedPath;

        var pointsT = new List<(BezierSplineVertex a, BezierSplineVertex b, float time)>();

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
        else if (Mode == BezierSplineGenMode.NoBezier)
        {
            for (int i = 0; i < (vertices.Length - 1); ++i)
            {
                pointsT.Add((vertices[i], vertices[i + 1], 0));
            }
        }
        else
        {
            var lengthStep = length / (NumPoints - 1);
            var currentLength = 0f;
            while (currentLength < length && pointsT.Count < (NumPoints-1))
            {
                var point = GetPointOnPath(currentLength);
                var pos = GetPosition(point.a, point.b, point.time);

                pointsT.Add(point);
                currentLength += lengthStep;
            }
        }

        // add end
        var lastPoint = pointsT.LastOrDefault();
        if (lastPoint.b != vertices[vertices.Length - 1] || lastPoint.time < 1)
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

        _cachedPath = pointsT;
        _cachedPathHash = hash;
        return pointsT;
    }

    public Vector3[] ComputePath()
    {
        var pointsT = ComputePathPoints();
        if (pointsT == null) return null;

        // build points
        if (Tristrip)
        {
            Vector3[] points = new Vector3[pointsT.Count];
            for (int i = 0; i < pointsT.Count; i++)
            {
                var idx = i;
                var p0 = GetPosition(pointsT[i].a, pointsT[i].b, pointsT[i].time);
                var bitangent = GetBitangent(pointsT[i].a, pointsT[i].b, pointsT[i].time);

                points[idx + 0] = p0 + (bitangent * TristripWidth) * ((i % 2 == 0) ? 1 : -1);
            }

            return points;
        }
        else
        {
            Vector3[] points = new Vector3[pointsT.Count];
            for (int i = 0; i < pointsT.Count; ++i)
            {
                points[i] = GetPosition(pointsT[i].a, pointsT[i].b, pointsT[i].time);
            }

            return points;
        }
    }

    public Vector3 GetPosition(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        if (Mode == BezierSplineGenMode.NoBezier) return t >= 1 ? b.Control : a.Control;
        if (t <= 0) return a.Control;
        if (t >= 1) return b.Control;

        float u = 1 - t;
        return (a.Control * Mathf.Pow(u, 3)) +
            (a.HandleOut * 3 * t * Mathf.Pow(u, 2)) +
            (b.HandleIn * 3 * Mathf.Pow(t, 2) * u) +
            (b.Control * Mathf.Pow(t, 3));
    }

    public Vector3 GetTangent(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        if (Mode == BezierSplineGenMode.NoBezier) return (b.Control - a.Control).normalized;

        var u = 1 - t;
        var p0 = 3 * (a.HandleOut - a.Control);
        var p1 = 3 * (b.HandleIn - a.HandleOut);
        var p2 = 3 * (b.Control - b.HandleIn);
        return p0 * Mathf.Pow(u, 2) +
               2 * p1 * u * t +
               p2 * Mathf.Pow(t, 2);
    }

    public Vector3 GetNormal(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        if (Mode == BezierSplineGenMode.NoBezier) return Vector3.Cross((b.Control - a.Control), Vector3.right).normalized;

        var tangent = GetTangent(a, b, t);
        float invT = 1 - t;
        var secondDeriv = 6 * invT * (b.HandleIn - 2 * a.HandleOut + a.Control) +
               6 * t * (b.Control - 2 * b.HandleIn + a.HandleOut);

        return Vector3.Cross(Vector3.Cross(tangent, secondDeriv), tangent).normalized;
    }

    public Vector3 GetAlignedNormal(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        var alignedNormal = this.transform.rotation * AlignedNormal;
        var vertices = this.GetVertices();
        if (vertices.Length < 2) return alignedNormal;

        var tangent = GetTangent(a, b, t);
        var normal = Vector3.Cross(Vector3.Cross(alignedNormal, tangent), tangent).normalized;
        if (normal.sqrMagnitude == 0f)
        {
            // fallback to cross product with right or forward
            normal = Vector3.Cross(tangent, this.transform.right).normalized;
            if (normal.sqrMagnitude == 0f) normal = Vector3.Cross(tangent, this.transform.forward).normalized;
        }

        //if (Mode == BezierSplineGenMode.NoBezier)
        //{
        //    if (normal.sqrMagnitude == 0f) normal = Vector3.Cross(vertices[1].Control - vertices[0].Control, this.transform.right).normalized;
        //    if (normal.sqrMagnitude == 0f) normal = Vector3.Cross(vertices[1].Control - vertices[0].Control, this.transform.forward).normalized;
        //    return Vector3.Cross((b.Control - a.Control), normal).normalized;
        //}

        return normal;
    }

    public Vector3 GetBitangent(BezierSplineVertex a, BezierSplineVertex b, float t)
    {
        var tan = GetTangent(a, b, t);
        var normal = GetAlignedNormal(a, b, t);
        return Vector3.Cross(tan, normal).normalized;
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

        var count = path.Length;
        for (int i = 0; i < count; ++i)
        {
            var nextI = i + 1;
            var pos = path[i];
            var pos2 = nextI >= count ? path[0] : path[nextI];
            if (nextI >= count && !Loop) pos2 = pos;
            var tan = (pos2 - pos).normalized;
            if (tan == Vector3.zero) tan = i > 0 ? (path[i] - path[i - 1]).normalized : this.transform.forward;
            var normal = Vector3.up; // GetNormal(Points[i], Points[i + 1], t);
            var bitangent = Vector3.Cross(tan, normal);
            var rot = Quaternion.LookRotation(tan, normal);
            normal = -Vector3.Cross(tan, bitangent);

            Gizmos.color = Color.white;
            Gizmos.DrawLine(pos, pos2);

            if (DrawPoints)
            {
                Gizmos.DrawSphere(pos, 0.2f);
                Handles.Label(pos + Vector3.up * 0.4f, $"{i}");
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
