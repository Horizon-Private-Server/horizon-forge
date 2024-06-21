using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[CustomEditor(typeof(MapRender))]
public class MapRenderEditor : Editor
{
    static int[] RENDER_SCALE_OPTIONS_VALUES = new[]
    {
        128,
        256,
        512,
        1024,
        2048,
        4096,
    };

    static string[] RENDER_SCALE_OPTIONS_LABELS = new[]
    {
        "128x128",
        "256x256",
        "512x512",
        "1024x1024",
        "2048x2048",
        "4096x4096",
    };

    SerializedProperty m_RenderScale;
    SerializedProperty m_BackgroundColor;


    private void OnEnable()
    {
        m_RenderScale = serializedObject.FindProperty("RenderScale");
        m_BackgroundColor = serializedObject.FindProperty("BackgroundColor");
    }

    public override void OnInspectorGUI()
    {
        var mapRender = this.serializedObject.targetObject as MapRender;

        //base.OnInspectorGUI();
        serializedObject.Update();

        m_RenderScale.intValue = EditorGUILayout.IntPopup("Render Size", m_RenderScale.intValue, RENDER_SCALE_OPTIONS_LABELS, RENDER_SCALE_OPTIONS_VALUES);
        EditorGUILayout.PropertyField(m_BackgroundColor);

        serializedObject.ApplyModifiedProperties();

        GUILayout.Space(20);
        if (GUILayout.Button("Render"))
        {
            render(mapRender);
        }
        if (GUILayout.Button("Render Depth"))
        {
            render(mapRender, depth: true);
        }

        var p = mapRender.transform.position + mapRender.transform.rotation * mapRender.transform.localScale;

        GUILayout.Label($"Minimap Coordinates:\npX: {mapRender.transform.position.x}\npY: {mapRender.transform.position.z}\nsX: {mapRender.transform.localScale.x}\nsY: {mapRender.transform.localScale.z}");
    }

    private void render(MapRender mapRender, bool depth = false)
    {
        var scene = SceneManager.GetActiveScene();
        if (scene == null) return;

        var resourcesFolder = FolderNames.GetMapFolder(scene.name);
        var outPath = EditorUtility.SaveFilePanelInProject("Save Minimap Render", "minimap", "png", "", resourcesFolder);
        if (String.IsNullOrEmpty(outPath)) return;
        var fi = new FileInfo(outPath);

        // setup render texture
        int width = mapRender.RenderScale;
        float aspectRatio = mapRender.transform.localScale.z / mapRender.transform.localScale.x;

        RenderTexture rt = new RenderTexture(width, (int)(width * aspectRatio), 0, RenderTextureFormat.ARGB64, 0);

        var mapRenderLayers = FindObjectsOfType<MapRenderLayer>();

        try
        {
            if (depth)
                Shader.EnableKeyword("_DEPTH");
            else
                Shader.EnableKeyword("_MAPRENDER");

            // pre layers
            if (mapRenderLayers != null)
                foreach (var layer in mapRenderLayers)
                    layer.OnPreMapRender();

            // setup camera
            mapRender.UpdateCamera();
            var camera = mapRender.GetComponent<Camera>();
            camera.targetTexture = rt;
            //mapRender.camera.depthTextureMode = depth ? DepthTextureMode.Depth : DepthTextureMode.None;
            //mapRender.camera.SetTargetBuffers(rt.colorBuffer, rt.depthBuffer);

            camera.Render();


            SaveTexture(rt, outPath);

        }
        finally
        {
            Shader.DisableKeyword("_MAPRENDER");
            Shader.DisableKeyword("_DEPTH");
            rt.Release();

            // post layers
            if (mapRenderLayers != null)
                foreach (var layer in mapRenderLayers)
                    layer.OnPostMapRender();
        }
    }

    private void SaveTexture(RenderTexture rt, string path)
    {
        Texture2D tex = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false);
        RenderTexture.active = rt;
        tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        tex.Apply();

        byte[] bytes = tex.EncodeToPNG();
        System.IO.File.WriteAllBytes(path, bytes);
        AssetDatabase.ImportAsset(path);
    }
}
