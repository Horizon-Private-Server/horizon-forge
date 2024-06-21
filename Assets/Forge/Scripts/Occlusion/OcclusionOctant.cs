using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public class OcclusionOctant : MonoBehaviour
{
    [SerializeField, HideInInspector]
    public List<Vector3> Octants;

    private void OnDrawGizmosSelected()
    {
        foreach (var octant in Octants)
            Gizmos.DrawWireCube(octant + Vector3.one * 2f, Vector3.one * 0.5f);
    }

    private void OnDrawGizmos()
    {

    }

}
