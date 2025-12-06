using UnityEngine;
using UnityEditor;
using System.IO;
using System;

public class TintTextureWindow : EditorWindow
{
    static string _lastSaveFilePath = null;

    Texture2D inputTexture;
    Color tintColor = Color.white;
    float tintAlpha = 1f;

    Texture2D lastInputTexture = null;
    Color32 inputTextureMinColor = Color.white;
    Color32 inputTextureMaxColor = Color.clear;

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

        RenderTextureInfo();

        GUI.enabled = true;
    }

    void RenderTextureInfo()
    {
        if (!inputTexture) return;

        if (inputTexture != lastInputTexture)
        {
            var tex = inputTexture;
            if (!inputTexture.isReadable)
                tex = UnityHelper.CloneTexture(inputTexture);

            var pixels = tex.GetPixels();
            Color min = Color.white;
            Color max = Color.clear;
            foreach (var p in pixels)
            {
                if (p.r < min.r) min.r = p.r;
                if (p.g < min.g) min.g = p.g;
                if (p.b < min.b) min.b = p.b;
                if (p.a < min.a) min.a = p.a;

                if (p.r > max.r) max.r = p.r;
                if (p.g > max.g) max.g = p.g;
                if (p.b > max.b) max.b = p.b;
                if (p.a > max.a) max.a = p.a;
            }

            inputTextureMinColor = (Color32)min;
            inputTextureMaxColor = (Color32)max;
            lastInputTexture = inputTexture;
        }

        GUILayout.Space(20);
        GUILayout.Label("Min Color: " + inputTextureMinColor);
        GUILayout.Label("Max Color: " + inputTextureMaxColor);
    }

    void BakeAndSave()
    {
        if (inputTexture == null)
            return;

        string path = EditorUtility.SaveFilePanel(
            "Save Tinted Texture",
            string.IsNullOrEmpty(_lastSaveFilePath) ? "" : Path.GetDirectoryName(_lastSaveFilePath),
            inputTexture.name + "_tinted.png",
            "png"
        );

        if (string.IsNullOrEmpty(path))
            return;

        _lastSaveFilePath = path;
        var tint = tintColor;
        tint.a = tintAlpha;
        var output = UnityHelper.CloneTexture(inputTexture, true, tint);
        byte[] pngData = output.EncodeToPNG();
        File.WriteAllBytes(path, pngData);
        AssetDatabase.Refresh();

        EditorUtility.DisplayDialog("Done", "Tinted texture saved!", "OK");
    }
}
