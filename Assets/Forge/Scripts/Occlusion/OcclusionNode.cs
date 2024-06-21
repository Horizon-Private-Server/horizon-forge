using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class OcclusionNode : MonoBehaviour
{
    [SerializeField, HideInInspector]
    public Matrix4x4 LastTRS;

    private OcclusionGraph graph;

    private void Start()
    {
        graph = GameObject.FindObjectOfType<OcclusionGraph>();
        if (graph != null)
            graph.RegisterNode(this);
    }

    private void OnDestroy()
    {
        if (graph)
            graph.UnregisterNode(this);
    }

    private void OnDrawGizmosSelected()
    {
        if (LastTRS != transform.localToWorldMatrix)
        {
            if (graph)
            {
                graph.RefreshCache();
            }
        }

        Gizmos.color = Color.red;
        Gizmos.DrawWireSphere(this.transform.position, 1f);
    }
}
