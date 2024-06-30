using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public class CollisionResultsVisualizer : MonoBehaviour
{
    private void OnEnable()
    {
        UpdateShaderGlobals();
    }

    private void OnDisable()
    {
        Shader.SetGlobalInteger("_COLLISION_RESULTS_BAD_SECTORS_COUNT", 0);
    }

    public void UpdateShaderGlobals()
    {
        var nodes = GetComponentsInChildren<CollisionResultsVisualizerNode>();
        var badSectors = new Vector4[1024];

        for (int i = 0; i < nodes.Length; i++)
        {
            badSectors[i] = nodes[i].transform.position;
        }

        Shader.SetGlobalVectorArray("_COLLISION_RESULTS_BAD_SECTORS", badSectors);
        Shader.SetGlobalInteger("_COLLISION_RESULTS_BAD_SECTORS_COUNT", nodes.Length);
    }

}
