using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Area : MonoBehaviour
{
    public float BSphereRadius;

    public List<Spline> Splines;
    public List<Cuboid> Cuboids;

    private void OnDrawGizmosSelected()
    {
        DrawGizmos();
    }

    private void DrawGizmos()
    {
        if (Cuboids != null)
        {
            foreach (var cuboid in Cuboids)
            {
                if (!cuboid) continue;
                UnityHelper.DrawLine(this.transform.position, cuboid.transform.position, Color.green, 2f);
            }
        }

        Gizmos.DrawWireSphere(this.transform.position, BSphereRadius);
    }
}
