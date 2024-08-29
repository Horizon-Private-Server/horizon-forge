using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(DockerManager))]
public class DockerManagerEditor : Editor
{
    string cmd = "ls";

    public override void OnInspectorGUI()
    {
        var manager = (target as DockerManager);
        base.OnInspectorGUI();

        GUILayout.Label(manager.GetStatus()?.ToString() ?? "null");
        if (GUILayout.Button("Start"))
        {
            manager.Run();
        }


        cmd = EditorGUILayout.TextField(cmd);
        if (GUILayout.Button("Test"))
        {
            _ = Task.Run(async () =>
            {
                var res = await manager.ExecuteAsync(cmd.Split(' '));
                Dispatcher.RunOnMainThread(() =>
                {
                    Debug.Log(res.ExitCode + ": " + res.Stdout);
                    if (!string.IsNullOrEmpty(res.Stderr))
                        Debug.LogError(res.Stderr);
                });
            });
        }
    }

}
