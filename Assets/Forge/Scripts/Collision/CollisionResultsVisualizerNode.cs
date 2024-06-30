using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public class CollisionResultsVisualizerNode : MonoBehaviour
{
    private void OnDrawGizmos()
    {
        Handles.color = Color.red;
        Handles.DrawWireCube(this.transform.position, Vector3.one * 4f);
    }
}
