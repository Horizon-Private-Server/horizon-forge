using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(UnityTerrainToTfrags)), CanEditMultipleObjects]
public class UnityTerrainToTfragsEditor : Editor
{
    private void OnEnable()
    {
        
    }

    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        if (this.targets != null && this.targets.Length > 1)
        {
            var firstTfragGen = this.targets[0] as UnityTerrainToTfrags;

            GUILayout.Space(20);
            EditorGUILayout.LabelField("Generation", EditorStyles.boldLabel);
            if (UnityTerrainToTfrags.m_RenderGenerated != EditorGUILayout.Toggle("Render Generated", UnityTerrainToTfrags.m_RenderGenerated))
            {
                var render = UnityTerrainToTfrags.m_RenderGenerated = !UnityTerrainToTfrags.m_RenderGenerated;
                var allUTTs = FindObjectsOfType<UnityTerrainToTfrags>();
                foreach (var tfragGen in allUTTs)
                {
                    tfragGen.SetVisible(render);
                }
            }

            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Generate"))
            {
                foreach (var target in this.targets)
                {
                    if (target is UnityTerrainToTfrags tfragGen)
                    {
                        tfragGen.Regenerate();
                    }
                }
            }
            if (GUILayout.Button("Generate All"))
            {
                var allUTTs = FindObjectsOfType<UnityTerrainToTfrags>();
                foreach (var utt in allUTTs)
                {
                    utt.Regenerate();
                }
            }
            GUILayout.EndHorizontal();
        }
        else if (target is UnityTerrainToTfrags tfragGen)
        {
            if (tfragGen.GetComponent<Terrain>() is Terrain terrain && terrain)
            {
                if (terrain.terrainData.terrainLayers.Length > 4)
                {
                    EditorGUILayout.HelpBox("Each terrain may only have up to 4 textures. Please consider adding an additional terrain if wish to use more.", MessageType.Error);
                }
            }

            GUILayout.Space(20);
            EditorGUILayout.LabelField("Generation", EditorStyles.boldLabel);
            if (UnityTerrainToTfrags.m_RenderGenerated != EditorGUILayout.Toggle("Render Generated", UnityTerrainToTfrags.m_RenderGenerated))
            {
                var render = UnityTerrainToTfrags.m_RenderGenerated = !UnityTerrainToTfrags.m_RenderGenerated;
                var allUTTs = FindObjectsOfType<UnityTerrainToTfrags>();
                foreach (var utt in allUTTs)
                {
                    utt.SetVisible(render);
                }
            }

            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Generate"))
            {
                tfragGen.Regenerate();
            }
            if (GUILayout.Button("Generate All"))
            {
                var allUTTs = FindObjectsOfType<UnityTerrainToTfrags>();
                foreach (var utt in allUTTs)
                {
                    utt.Regenerate();
                }
            }
            GUILayout.EndHorizontal();
        }
    }

}
