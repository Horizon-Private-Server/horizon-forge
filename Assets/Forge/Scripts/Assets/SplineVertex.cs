using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class SplineVertex : MonoBehaviour
{
    private Matrix4x4 _lastTRS = Matrix4x4.identity;
    private Spline _parentSpline = null;

    private void OnValidate()
    {
        var spline = GetComponentInParent<Spline>();
        if (spline)
        {
            spline.RefreshVertices();
        }
    }

    private void OnDestroy()
    {
        var spline = GetComponentInParent<Spline>();
        if (spline)
        {
            spline.RefreshVertices();
        }
    }

    private void OnDrawGizmos()
    {
        if (!_parentSpline || _parentSpline.Vertices == null || !_parentSpline.Vertices.Contains(this))
            _parentSpline = GetComponentInParent<Spline>();

        if (!_parentSpline) return;

        if (Spline.DrawSplineGizmos)
        {
            var selected = Spline.SelectedSplines.Contains(_parentSpline);
            Gizmos.color = selected ? Color.red : Color.white;
            DrawGizmos();

            if (selected && _lastTRS != this.transform.localToWorldMatrix) UpdateMobys();
        }
    }

    public void DrawGizmos()
    {
        Gizmos.DrawSphere(this.transform.position, 1f);
    }

    private void UpdateMobys()
    {
        _lastTRS = this.transform.localToWorldMatrix;

        var spline = GetComponentInParent<Spline>();
        if (!spline) return;

        var mobys = FindObjectsOfType<Moby>();
        if (mobys != null)
        {
            foreach (var moby in mobys)
            {
                if (moby.PVarReferences != null && moby.PVarReferences.ContainsValue(spline))
                {
                    moby.UpdateAsset();
                }

                if (moby.PVarReferences != null && moby.PVarReferences.Select(x => x.Value as Area).Any(x => x && x.Splines != null && x.Splines.Contains(spline)))
                {
                    moby.UpdateAsset();
                }
            }
        }
    }
}
