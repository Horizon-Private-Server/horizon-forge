using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[AddComponentMenu("")]
public class Area : MonoBehaviour
{
    public float BSphereRadius;

    public List<Spline> Splines = new List<Spline>();
    public List<Cuboid> Cuboids = new List<Cuboid>();

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


    [MenuItem("GameObject/Forge/Misc/Area", priority = 10)]
    public static void CreateNew()
    {
        var go = new GameObject("Area");
        var moby = go.AddComponent<Area>();
        UnityHelper.OnAfterCreateGameObject(go);
    }

}
