using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(CodeManager))]
public class CodeManagerEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var manager = (target as CodeManager);
        base.OnInspectorGUI();

#if !DOCKER
        GUILayout.Space(20);
        EditorGUILayout.HelpBox("Please make sure that Docker is installed and then click the below button to activate it in Forge.", MessageType.Error);
        if (GUILayout.Button("Activate Docker"))
        {
            PlayerSettings.GetScriptingDefineSymbols(UnityEditor.Build.NamedBuildTarget.Standalone, out var defines);
            Array.Resize(ref defines, defines.Length + 1);
            defines[defines.Length - 1] = "DOCKER";
            PlayerSettings.SetScriptingDefineSymbols(UnityEditor.Build.NamedBuildTarget.Standalone, defines);
        }
        EditorGUILayout.HelpBox("NOTE: This process may take awhile, please wait until this message disappears.", MessageType.Warning);
#endif
    }

}
