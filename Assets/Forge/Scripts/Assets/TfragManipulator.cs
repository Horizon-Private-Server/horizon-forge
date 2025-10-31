using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class TfragManipulator : MonoBehaviour, IBuildHook
{
    public enum Type
    {
        Shift,
        Collapse,
    }

    public bool IsEnabled => this.isActiveAndEnabled;

    public Type ManipulateType;
    public Vector3 Offset;
    public bool AlignOffsetToBox;
    public float FalloffRadius = 1;
    public float Falloff = 0;

    public List<TfragChunk> Tfrags = new List<TfragChunk>();

    private Matrix4x4 _lastTRS;

    // make sure tfrag chunks are up to date on build
    public void Configure(BuildState state)
    {
        Register();
    }

    private void OnEnable()
    {
        Register();
    }

    private void OnDisable()
    {
        Unregister();
    }

    private void OnValidate()
    {
        Register();
    }

    private void Update()
    {
        if (Selection.activeGameObject != this.gameObject) return;

        if (_lastTRS != this.transform.localToWorldMatrix)
        {
            _lastTRS = this.transform.localToWorldMatrix;
            Register();
        }
    }

    private void OnDrawGizmosSelected()
    {
        var m = Gizmos.matrix;
        Gizmos.matrix = this.transform.localToWorldMatrix;
        Gizmos.color = Color.white * 0.5f;
        Gizmos.DrawCube(Vector3.zero, Vector3.one);
        Gizmos.color = Color.white * 0.5f;
        Gizmos.DrawSphere(Vector3.zero, FalloffRadius);
        Gizmos.matrix = m;

        Gizmos.color = Color.white;
        switch (ManipulateType)
        {
            case Type.Shift: Gizmos.DrawWireSphere(this.transform.position + GetOffset(), 0.1f); break;
            case Type.Collapse: Gizmos.DrawWireSphere(this.transform.position + GetOffset(), 0.1f); break;
        }
    }

    private void Register()
    {
        if (Tfrags != null)
        {
            foreach (var tfrag in Tfrags)
            {
                if (!tfrag) continue;

                tfrag.RegisterManipulator(this);
                tfrag.UpdateManipulators();
            }
        }
    }

    private void Unregister()
    {
        if (Tfrags != null)
        {
            foreach (var tfrag in Tfrags)
            {
                if (!tfrag) continue;

                tfrag.UnregisterManipulator(this);
                tfrag.UpdateManipulators();
            }
        }
    }

    private Vector4 GetMaterialValue()
    {
        switch (ManipulateType)
        {
            case Type.Shift: return GetOffset();
            case Type.Collapse: return this.transform.position + GetOffset();
            default: return Vector4.zero;
        }
    }

    public static void ApplyMaterial(MaterialPropertyBlock mpb, IEnumerable<TfragManipulator> manipulators)
    {
        if (manipulators?.Any() ?? false)
        {
            mpb.SetVectorArray("_ManipulatorValues", manipulators.Select(x => x.GetMaterialValue()).ToArray());
            mpb.SetFloatArray("_ManipulatorTypes", manipulators.Select(x => (float)x.ManipulateType).ToArray());
            mpb.SetFloatArray("_ManipulatorFalloffRadius", manipulators.Select(x => x.FalloffRadius).ToArray());
            mpb.SetFloatArray("_ManipulatorFalloffs", manipulators.Select(x => x.Falloff).ToArray());
            mpb.SetMatrixArray("_ManipulatorArray", manipulators.Select(x => x.transform.worldToLocalMatrix).ToArray());
            mpb.SetInteger("_ManipulatorArrayCount", manipulators.Count());
        }
        else
        {
            mpb.SetInteger("_ManipulatorArrayCount", 0);
        }
    }

    public void Apply(ref byte[] header, ref byte[] data)
    {
        switch (ManipulateType)
        {
            case Type.Shift: TfragHelper.Displace(header, data, transform.worldToLocalMatrix, GetOffset(), falloffRadius: FalloffRadius, falloff: Falloff); break;
            case Type.Collapse: TfragHelper.Collapse(header, data, transform.worldToLocalMatrix, this.transform.position + GetOffset(), falloffRadius: FalloffRadius, falloff: Falloff); break;
        }
    }

    private Vector3 GetOffset() => AlignOffsetToBox ? Matrix4x4.Rotate(transform.rotation).MultiplyPoint(Offset) : Offset;
}
