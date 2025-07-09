using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public class TfragSplineVertex : MonoBehaviour
{
    public enum Alignment
    {
        Inner,
        Middle,
        Outer
    }

    public Vector3 HandleIn => transform.TransformPoint(HandleInOffset);
    public Vector3 HandleOut => transform.TransformPoint(HandleOutOffset);
    public Vector3 Control => transform.TransformPoint(Vector3.zero);
    public bool IsDisconnected => false; // Disconnected;

    [Header("Handles")]
    [SerializeField]
    private Vector3 HandleInOffset = Vector3.back;
    [SerializeField]
    private Vector3 HandleOutOffset = Vector3.forward;

    [Header("Width")]
    [SerializeField]
    public Vector3 WidthDirection = Vector3.up;
    [SerializeField]
    public float Width = 5f;
    [SerializeField]
    public Alignment WidthAlignment = Alignment.Middle;
    [SerializeField]
    public BezierSpline WidthSpline;
    [SerializeField]
    public bool WidthSplineAbsolute;

    private TfragSpline ParentSpline;

    private void Start()
    {
        ParentSpline = GetComponentInParent<TfragSpline>();
        if (ParentSpline)
            ParentSpline.InvalidateCache();
    }

    private void OnValidate()
    {
        ParentSpline = GetComponentInParent<TfragSpline>();
        if (ParentSpline)
            ParentSpline.InvalidateCache();
    }

    private void OnDrawGizmosSelected()
    {
        DrawGizmos();
    }

    private void OnDrawGizmos()
    {
        if (Selection.activeGameObject == this.gameObject) return;
        if (!ParentSpline) return;

        if (ParentSpline.IsSplineSelected())
            DrawGizmos();
    }

    private void DrawGizmos()
    {
        var alpha = Selection.activeGameObject == this.gameObject ? 1f : 0.5f;

        Gizmos.color = Color.blue * alpha;
        Gizmos.DrawSphere(transform.position, 1f);
        Gizmos.color = Color.green * alpha;
        Gizmos.DrawSphere(HandleIn, 0.5f);
        Gizmos.color = Color.red * alpha;
        Gizmos.DrawSphere(HandleOut, 0.5f);
    }

    public void SetOffsets(Vector3 handleIn, Vector3 handleOut)
    {
        HandleInOffset = handleIn;
        HandleOutOffset = handleOut;
    }

    public Vector3 GetConstantWidth(float t)
    {
        var width = Width * t;
        var widthOffset = Width * ((int)WidthAlignment - 2) * 0.5f;
        return (WidthDirection * width) + (WidthDirection * widthOffset);
    }

    public Vector3 GetConstantNormal(float t)
    {
        return Vector3.Cross(WidthDirection, this.HandleOutOffset).normalized;
    }

    public Vector3 GetSplineWidth(float t)
    {
        var path = WidthSpline.ComputePath();
        var i = Mathf.RoundToInt(t * (path.Length - 1));

        var point = path[i];
        return WidthSplineAbsolute ? this.transform.worldToLocalMatrix.MultiplyVector(point - WidthSpline.transform.position) : WidthSpline.transform.worldToLocalMatrix.MultiplyPoint(point);
    }

    public Vector3 GetSplineNormal(float t)
    {
        var path = WidthSpline.ComputePathPoints();
        var i = Mathf.RoundToInt(t * (path.Count - 1));

        var bitangent = WidthSpline.GetAlignedNormal(path[i].a, path[i].b, path[i].time).normalized;
        return WidthSplineAbsolute ? this.transform.worldToLocalMatrix.MultiplyVector(bitangent) : WidthSpline.transform.worldToLocalMatrix.MultiplyVector(bitangent);
    }
}
