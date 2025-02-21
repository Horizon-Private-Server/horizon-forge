using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[CustomEditor(typeof(MapConfig))]
public class MapConfigEditor : Editor
{
    static readonly string[] DLBaseMaps = ((DLMapIds[])Enum.GetValues(typeof(DLMapIds))).Where(x => (int)x > 40).Select(x => Enum.GetName(typeof(DLMapIds), x)).ToArray();
    static readonly string[] UYABaseMaps = ((UYAMapIds[])Enum.GetValues(typeof(UYAMapIds))).Where(x => (int)x >= 40).Select(x => Enum.GetName(typeof(UYAMapIds), x)).ToArray();

    SerializedProperty m_MapVersion;
    SerializedProperty m_MapName;
    SerializedProperty m_MapFilename;

    SerializedProperty m_DLBaseMap;
    SerializedProperty m_DLForceCustomMode;
    SerializedProperty m_DLLoadingScreen;
    SerializedProperty m_DLMinimap;
    SerializedProperty m_DLMobysIncludedInExport;

    SerializedProperty m_UYABaseMap;
    SerializedProperty m_UYAMinimap;
    SerializedProperty m_UYAMobysIncludedInExport;

    SerializedProperty m_ShrubMinRenderDistance;

    SerializedProperty m_DeathPlane;
    SerializedProperty m_RenderDeathPlane;

    SerializedProperty m_BackgroundColor;
    SerializedProperty m_FogColor;
    SerializedProperty m_FogNearDistance;
    SerializedProperty m_FogFarDistance;
    SerializedProperty m_FogNearIntensity;
    SerializedProperty m_FogFarIntensity;

    private void OnEnable()
    {
        m_MapVersion = serializedObject.FindProperty("MapVersion");
        m_MapName = serializedObject.FindProperty("MapName");
        m_MapFilename = serializedObject.FindProperty("MapFilename");

        m_DLBaseMap = serializedObject.FindProperty("DLBaseMap");
        m_DLForceCustomMode = serializedObject.FindProperty("DLForceCustomMode");
        m_DLLoadingScreen = serializedObject.FindProperty("DLLoadingScreen");
        m_DLMinimap = serializedObject.FindProperty("DLMinimap");
        m_DLMobysIncludedInExport = serializedObject.FindProperty("DLMobysIncludedInExport");

        m_UYABaseMap = serializedObject.FindProperty("UYABaseMap");
        m_UYAMinimap = serializedObject.FindProperty("UYAMinimap");
        m_UYAMobysIncludedInExport = serializedObject.FindProperty("UYAMobysIncludedInExport");

        m_ShrubMinRenderDistance = serializedObject.FindProperty("ShrubMinRenderDistance");

        m_DeathPlane = serializedObject.FindProperty("DeathPlane");
        m_RenderDeathPlane = serializedObject.FindProperty("RenderDeathPlane");

        m_BackgroundColor = serializedObject.FindProperty("BackgroundColor");
        m_FogColor = serializedObject.FindProperty("FogColor");
        m_FogNearDistance = serializedObject.FindProperty("FogNearDistance");
        m_FogFarDistance = serializedObject.FindProperty("FogFarDistance");
        m_FogNearIntensity = serializedObject.FindProperty("FogNearIntensity");
        m_FogFarIntensity = serializedObject.FindProperty("FogFarIntensity");
    }

    public override void OnInspectorGUI()
    {
        var mapConfig = target as MapConfig;

        //base.OnInspectorGUI();

        serializedObject.Update();

        // map build
        EditorGUILayout.PropertyField(m_MapVersion);
        EditorGUILayout.PropertyField(m_MapName);
        EditorGUILayout.PropertyField(m_MapFilename);

        // deadlocked
        EditorGUILayout.PropertyField(m_DLBaseMap);
        if ((target as MapConfig).HasDeadlockedBaseMap())
        {
            EditorGUILayout.PropertyField(m_DLForceCustomMode);
            EditorGUILayout.PropertyField(m_DLLoadingScreen);
            EditorGUILayout.PropertyField(m_DLMinimap);
            EditorGUILayout.PropertyField(m_DLMobysIncludedInExport);
        }
        else
        {
            GUILayout.BeginHorizontal();
            mapConfig.ImportDLBaseMapIdx = EditorGUILayout.Popup(mapConfig.ImportDLBaseMapIdx, DLBaseMaps);
            if (GUILayout.Button("Set Base Map"))
                AddBaseMap_DL(mapConfig);
            GUILayout.EndHorizontal();
        }

        // uya
        EditorGUILayout.PropertyField(m_UYABaseMap);
        if ((target as MapConfig).HasUYABaseMap())
        {
            EditorGUILayout.PropertyField(m_UYAMinimap);
            EditorGUILayout.PropertyField(m_UYAMobysIncludedInExport);
        }
        else
        {
            GUILayout.BeginHorizontal();
            mapConfig.ImportUYABaseMapIdx = EditorGUILayout.Popup(mapConfig.ImportUYABaseMapIdx, UYABaseMaps);
            if (GUILayout.Button("Set Base Map"))
                AddBaseMap_UYA(mapConfig);
            GUILayout.EndHorizontal();
        }

        // render settings
        EditorGUILayout.PropertyField(m_ShrubMinRenderDistance);

        // world settings
        EditorGUILayout.PropertyField(m_DeathPlane);
        EditorGUILayout.PropertyField(m_RenderDeathPlane);

        // fog
        EditorGUILayout.PropertyField(m_BackgroundColor);
        EditorGUILayout.PropertyField(m_FogColor);
        EditorGUILayout.PropertyField(m_FogNearDistance);
        EditorGUILayout.PropertyField(m_FogFarDistance);
        EditorGUILayout.PropertyField(m_FogNearIntensity);
        EditorGUILayout.PropertyField(m_FogFarIntensity);

        serializedObject.ApplyModifiedProperties();

        GUILayout.Space(20);
        if (GUILayout.Button("Open Build Folder"))
        {
            var path = FolderNames.GetMapBuildFolder(SceneManager.GetActiveScene().name) + "/";
            EditorUtility.RevealInFinder(path);
        }
    }

    private void AddBaseMap_DL(MapConfig mapConfig)
    {
        LevelImporterWindow importerWindow = new LevelImporterWindow();
        importerWindow.ImportBaseMap(mapConfig, (int)Enum.Parse<DLMapIds>(DLBaseMaps[mapConfig.ImportDLBaseMapIdx]), RCVER.DL);
    }

    private void AddBaseMap_UYA(MapConfig mapConfig)
    {
        LevelImporterWindow importerWindow = new LevelImporterWindow();
        importerWindow.ImportBaseMap(mapConfig, (int)Enum.Parse<UYAMapIds>(UYABaseMaps[mapConfig.ImportUYABaseMapIdx]), RCVER.UYA);
    }

}
