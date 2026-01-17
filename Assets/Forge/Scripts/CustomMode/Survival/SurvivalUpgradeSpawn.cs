using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class SurvivalUpgradeSpawn : MonoBehaviour
{
    private void OnDrawGizmos()
    {
        var m = Gizmos.matrix;

        Gizmos.matrix = this.transform.localToWorldMatrix;
        Gizmos.DrawWireCube(Vector3.zero, new Vector3(0.5f, 0.5f, 0));
        Gizmos.matrix = m;
    }
}
