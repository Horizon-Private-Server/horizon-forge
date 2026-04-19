using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(ConvertToTfrags)), CanEditMultipleObjects]
public class ConvertToTfragsEditor : Editor
{
    private void OnEnable()
    {
        
    }

    public override void OnInspectorGUI()
    {
		EditorGUILayout.HelpBox("Tfrags generation is limited. Too many tfrags can result in rendering bugs.", MessageType.Info);
		GUILayout.Space(20);

        base.OnInspectorGUI();

        if (this.targets != null && this.targets.Length > 1)
        {
            var firstTfragGen = this.targets[0] as ConvertToTfrags;

            GUILayout.Space(20);
            EditorGUILayout.LabelField("Generation", EditorStyles.boldLabel);
            if (ConvertToTfrags.m_RenderGenerated != EditorGUILayout.Toggle("Render Generated", ConvertToTfrags.m_RenderGenerated))
            {
                var render = ConvertToTfrags.m_RenderGenerated = !ConvertToTfrags.m_RenderGenerated;
                var allUTTs = FindObjectsOfType<ConvertToTfrags>();
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
                    if (target is ConvertToTfrags tfragGen)
                    {
                        tfragGen.Regenerate();
                    }
                }
            }
            if (GUILayout.Button("Generate All"))
            {
                var allUTTs = FindObjectsOfType<ConvertToTfrags>();
                foreach (var utt in allUTTs)
                {
                    utt.Regenerate();
                }
            }
            GUILayout.EndHorizontal();
        }
        else if (target is ConvertToTfrags tfragGen)
        {
            GUILayout.Space(20);
            EditorGUILayout.LabelField("Generation", EditorStyles.boldLabel);
            if (ConvertToTfrags.m_RenderGenerated != EditorGUILayout.Toggle("Render Generated", ConvertToTfrags.m_RenderGenerated))
            {
                var render = ConvertToTfrags.m_RenderGenerated = !ConvertToTfrags.m_RenderGenerated;
                var allUTTs = FindObjectsOfType<ConvertToTfrags>();
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
                var allUTTs = FindObjectsOfType<ConvertToTfrags>();
                foreach (var utt in allUTTs)
                {
                    utt.Regenerate();
                }
            }
            GUILayout.EndHorizontal();
        }

		// stats
		var tfrags = this.targets.Select(x => x as ConvertToTfrags);
		var objCount = tfrags.Count();
		var totalTfragCount = tfrags.Sum(x => x.TfragCount);
		var avgTfragCount = totalTfragCount / (float)objCount;
		GUILayout.Space(20);
		EditorGUILayout.HelpBox($"Total Selected Tfrags: {totalTfragCount}\nAverage Tfrags per Object: {avgTfragCount}", MessageType.Info);
    }

}
