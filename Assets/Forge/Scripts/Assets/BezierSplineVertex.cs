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

    private Hash128 _lastHash;
    private BezierSpline ParentSpline;

    private void Start()
    {
        SendRebuildUpstream();
    }

    private void OnValidate()
    {
        Dispatcher.RunOnMainThread(() => SendRebuildUpstream());
    }

    private void SendRebuildUpstream()
    {
        ParentSpline = GetComponentInParent<BezierSpline>();
        if (ParentSpline)
            ParentSpline.RebuildSpline();
    }

    private void OnDrawGizmosSelected()
    {
        var hash = ComputeHash();
        if (hash != _lastHash)
        {
            _lastHash = hash;
            SendRebuildUpstream();
        }

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
        Gizmos.DrawSphere(transform.position, 0.2f);
        Gizmos.color = Color.green * alpha;
        Gizmos.DrawSphere(HandleIn, 0.1f);
        Gizmos.color = Color.red * alpha;
        Gizmos.DrawSphere(HandleOut, 0.1f);
    }

    private Hash128 ComputeHash()
    {
        Hash128 hash = new Hash128();

        hash = hash.Append(Control);
        hash = hash.Append(HandleIn);
        hash = hash.Append(HandleOut);

        return hash;
    }

    public void SetOffsets(Vector3 handleIn, Vector3 handleOut)
    {
        HandleInOffset = handleIn;
        HandleOutOffset = handleOut;
    }
}
