using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(OcclusionVolume))]
public class OcclusionVolumeEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var volume = this.serializedObject.targetObject as OcclusionVolume;

        base.OnInspectorGUI();

        GUILayout.Space(20);

        volume.ValidateCache();

        var octantCount = volume.GetOctantCount();
        if (octantCount.HasValue)
        {
            GUILayout.Label("Octants: " + octantCount.Value.ToString());
        }

        if (GUILayout.Button("Refresh Cache"))
        {
            volume.RefreshCache();
        }
    }
}
