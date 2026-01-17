using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(SurvivalMobsScriptableObject))]
public class SurvivalMobsScriptableObjectEditor : Editor
{
    private SerializedProperty m_MobsProperty;
    private SerializedProperty m_PatchesProperty;
    private SerializedProperty m_WeaponStatsProperty;
    private SerializedProperty m_SurvivalMysteryBoxSprites;
    private SerializedProperty m_SurvivalStackableSprites;
    private SerializedProperty m_SurvivalBlessingSprites;

    private void OnEnable()
    {
        m_MobsProperty = serializedObject.FindProperty("Mobs");
        m_PatchesProperty = serializedObject.FindProperty("Patches");
        m_WeaponStatsProperty = serializedObject.FindProperty("WeaponStats");
        m_SurvivalMysteryBoxSprites = serializedObject.FindProperty("SurvivalMysteryBoxSprites");
        m_SurvivalStackableSprites = serializedObject.FindProperty("SurvivalStackableSprites");
        m_SurvivalBlessingSprites = serializedObject.FindProperty("SurvivalBlessingSprites");
    }


    public override void OnInspectorGUI()
    {
        var manager = (target as SurvivalMobsScriptableObject);
        //base.OnInspectorGUI();

        serializedObject.Update();

        m_MobsProperty.isExpanded = EditorGUILayout.Toggle("Mobs", m_MobsProperty.isExpanded);
        if (m_MobsProperty.isExpanded)
        {
            EditorGUI.indentLevel++;
            foreach (SurvivalMob mob in Enum.GetValues(typeof(SurvivalMob)))
            {
                var idx = manager.Mobs.FindIndex(x => x.Mob == mob);
                if (idx < 0)
                {
                    manager.Mobs.Add(new SurvivalMobsScriptableObject.SurvivalMobsConfig()
                    {
                        Mob = mob,
                        Variants = new List<SurvivalMobsScriptableObject.SurvivalMobVariant>()
                    {
                        new SurvivalMobsScriptableObject.SurvivalMobVariant("Normal", 0, DLMapIds.SP_Battledome, 0)
                    }
                    });
                    EditorUtility.SetDirty(target);
                    idx = manager.Mobs.Count - 1;
                }

                EditorGUILayout.PropertyField(m_MobsProperty.GetArrayElementAtIndex(idx), new GUIContent(mob.ToString()));
            }
            EditorGUI.indentLevel--;
        }
        
        //EditorGUILayout.PropertyField(m_MobsProperty);
        EditorGUILayout.PropertyField(m_PatchesProperty);
        EditorGUILayout.PropertyField(m_WeaponStatsProperty);
        EditorGUILayout.PropertyField(m_SurvivalMysteryBoxSprites);
        EditorGUILayout.PropertyField(m_SurvivalStackableSprites);
        EditorGUILayout.PropertyField(m_SurvivalBlessingSprites);
        serializedObject.ApplyModifiedProperties();
    }

}
