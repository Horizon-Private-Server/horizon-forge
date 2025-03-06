using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class BezierSplineVertex : MonoBehaviour
{
    public Vector3 HandleIn => transform.TransformPoint(HandleInOffset);
    public Vector3 HandleOut => transform.TransformPoint(HandleOutOffset);
    public Vector3 Control => transform.TransformPoint(Vector3.zero);
    public bool IsDisconnected => false; // Disconnected;

    [SerializeField]
    private Vector3 HandleInOffset = Vector3.back;
    [SerializeField]
    private Vector3 HandleOutOffset = Vector3.forward;
    //[SerializeField]
    //private bool Disconnected;


    private BezierSpline ParentSpline;

    private void Start()
    {
        ParentSpline = GetComponentInParent<BezierSpline>();
        if (ParentSpline)
            ParentSpline.InvalidateCache();
    }

    private void OnValidate()
    {
        ParentSpline = GetComponentInParent<BezierSpline>();
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
        Gizmos.color = Color.blue;
        Gizmos.DrawSphere(transform.position, 1f);
        Gizmos.color = Color.green;
        Gizmos.DrawSphere(HandleIn, 0.5f);
        Gizmos.color = Color.red;
        Gizmos.DrawSphere(HandleOut, 0.5f);
    }

    public void SetOffsets(Vector3 handleIn, Vector3 handleOut)
    {
        HandleInOffset = handleIn;
        HandleOutOffset = handleOut;
    }
}
