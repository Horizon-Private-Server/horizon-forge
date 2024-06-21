using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

[ExecuteInEditMode]
public class OcclusionViewer : MonoBehaviour
{
    private Vector3 lastOctant = Vector3.zero;
    private Vector3 lastGizmosOctant = Vector3.zero;
    private bool gizmosOctantInOctants = false;
    private MaterialPropertyBlock mpb;

    public int VisibleCount { get; private set; }

    private void OnEnable()
    {
        FadeNotVisible();
    }

    private void OnDisable()
    {
        RemoveFade();
    }

    private void OnDrawGizmosSelected()
    {
        var octant = GetCurrentOctant();
        if (octant != lastGizmosOctant)
        {
            var octants = UnityHelper.GetAllOctants();
            gizmosOctantInOctants = octants.Contains(octant);
            lastGizmosOctant = octant;
        }

        // draw octant we're in
        Gizmos.color = gizmosOctantInOctants ? Color.blue : Color.red;
        Gizmos.DrawWireCube(octant + Vector3.one * 2, Vector3.one * 4);
        Gizmos.color = Color.green;
        Gizmos.DrawWireCube(lastOctant + Vector3.one * 2, Vector3.one * 4);
    }

    void FadeNotVisible()
    {
        var octant = lastOctant = GetCurrentOctant();

        if (mpb == null)
            mpb = new MaterialPropertyBlock();

        VisibleCount = 0;
        var datas = FindObjectsOfType<Tie>();
        if (datas != null)
        {
            foreach (var data in datas)
            {
                var inOctant = data.Octants.Contains(octant);
                var mrs = data.GetRenderers();
                if (mrs != null)
                {
                    foreach (var mr in mrs)
                    {
                        mr.GetPropertyBlock(mpb);
                        mpb.SetInt("_Faded", inOctant ? 0 : 1);
                        mr.SetPropertyBlock(mpb);
                    }
                }

                if (inOctant)
                    VisibleCount++;
            }
        }
    }

    void RemoveFade()
    {
        VisibleCount = 0;
        var datas = FindObjectsOfType<Tie>();
        if (datas != null)
        {
            foreach (var data in datas)
            {
                var mrs = data.GetRenderers();
                if (mrs != null)
                {
                    foreach (var mr in mrs)
                    {
                        mr.SetPropertyBlock(null);
                    }
                }
            }

            VisibleCount = datas.Length;
        }
    }

    Vector3 GetCurrentOctant()
    {
        return new Vector3((int)(this.transform.position.x / 4.0f) * 4, (int)(this.transform.position.y / 4.0f) * 4, (int)(this.transform.position.z / 4.0f) * 4);
    }
}
