using System;
using UnityEngine;

[Serializable]
public class OcclusionData : ScriptableObject
{
    public Vector3[] Octants = new Vector3[0];
}
