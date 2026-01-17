using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(TfragSpline)), CanEditMultipleObjects]
public class TfragSplineEditor : Editor
{
    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        if (this.targets != null && this.targets.Length > 1)
        {
            var firstTfragSpline = this.targets[0] as TfragSpline;

            GUILayout.Space(20);
            EditorGUILayout.LabelField("Generation", EditorStyles.boldLabel);
            if (GUILayout.Button("Generate"))
            {
                foreach (var target in this.targets)
                {
                    if (target is TfragSpline tfragGen)
                    {
                        tfragGen.Regenerate();
                    }
                }
            }
        }
        else if (target is TfragSpline tfragGen)
        {
            //if (tfragGen.GetComponent<Terrain>() is Terrain terrain && terrain)
            //{
            //    if (terrain.terrainData.terrainLayers.Length > 4)
            //    {
            //        EditorGUILayout.HelpBox("Each terrain may only have up to 4 textures. Please consider adding an additional terrain if wish to use more.", MessageType.Error);
            //    }
            //}

            GUILayout.Space(20);
            EditorGUILayout.LabelField("Generation", EditorStyles.boldLabel);

            if (GUILayout.Button("Generate"))
            {
                tfragGen.Regenerate();
            }
        }
    }
}
