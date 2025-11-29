using UnityEngine;
using UnityEditor;
using System.IO;

public class TintTextureWindow : EditorWindow
{
    Texture2D inputTexture;
    Color tintColor = Color.white;
    float tintAlpha = 1f;

    [MenuItem("Tools/Tint Texture")]
    public static void ShowWindow()
    {
        GetWindow<TintTextureWindow>("Tint Texture");
    }

    void OnGUI()
    {
        GUILayout.Label("Tint Texture Tool", EditorStyles.boldLabel);

        inputTexture = (Texture2D)EditorGUILayout.ObjectField("Input Texture", inputTexture, typeof(Texture2D), false);
        tintColor = EditorGUILayout.ColorField(new GUIContent("Tint Color"), tintColor, true, false, false);
        tintAlpha = EditorGUILayout.FloatField("Tint Alpha", tintAlpha);

        GUI.enabled = inputTexture != null;

        if (GUILayout.Button("Bake & Save As PNG"))
        {
            BakeAndSave();
        }

        GUI.enabled = true;
    }

    void BakeAndSave()
    {
        if (inputTexture == null)
            return;

        string path = EditorUtility.SaveFilePanel(
            "Save Tinted Texture",
            "",
            inputTexture.name + "_tinted.png",
            "png"
        );

        if (string.IsNullOrEmpty(path))
            return;

        var tint = tintColor;
        tint.a = tintAlpha;
        var output = UnityHelper.CloneTexture(inputTexture, true, tint);
        byte[] pngData = output.EncodeToPNG();
        File.WriteAllBytes(path, pngData);
        AssetDatabase.Refresh();

        EditorUtility.DisplayDialog("Done", "Tinted texture saved!", "OK");
    }
}
