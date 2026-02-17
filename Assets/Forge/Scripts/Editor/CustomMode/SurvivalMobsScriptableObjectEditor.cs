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
    private SerializedProperty m_SurvivalDefaultItems;

    private void OnEnable()
    {
        m_MobsProperty = serializedObject.FindProperty("Mobs");
        m_PatchesProperty = serializedObject.FindProperty("Patches");
        m_WeaponStatsProperty = serializedObject.FindProperty("WeaponStats");
        m_SurvivalMysteryBoxSprites = serializedObject.FindProperty("SurvivalMysteryBoxSprites");
        m_SurvivalStackableSprites = serializedObject.FindProperty("SurvivalStackableSprites");
        m_SurvivalDefaultItems = serializedObject.FindProperty("SurvivalDefaultItems");
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

        // default items
        m_SurvivalDefaultItems.isExpanded = EditorGUILayout.Toggle("Survival Default Items", m_SurvivalDefaultItems.isExpanded);
        if (m_SurvivalDefaultItems.isExpanded)
        {
            EditorGUI.indentLevel++;
            var defaultItems = ((SurvivalDefaultItems[])Enum.GetValues(typeof(SurvivalDefaultItems))).OrderBy(x => x.ToString()).ToArray();
            foreach (SurvivalDefaultItems defaultItem in defaultItems)
            {
                var idx = manager.SurvivalDefaultItems.FindIndex(x => x.Item == defaultItem);
                if (idx < 0)
                {
                    manager.SurvivalDefaultItems.Add(new SurvivalMobsScriptableObject.SurvivalDefaultItem()
                    {
                        Item = defaultItem,
                        Def = new SurvivalItemEntry()
                        {
                            Name = defaultItem.ToString(),
                        }
                    });

                    EditorUtility.SetDirty(target);
                    idx = manager.SurvivalDefaultItems.Count - 1;
                }

                EditorGUILayout.PropertyField(m_SurvivalDefaultItems.GetArrayElementAtIndex(idx), new GUIContent(defaultItem.ToString()));
            }
            EditorGUI.indentLevel--;
        }

        //EditorGUILayout.PropertyField(m_MobsProperty);
        EditorGUILayout.PropertyField(m_PatchesProperty);
        EditorGUILayout.PropertyField(m_WeaponStatsProperty);
        EditorGUILayout.PropertyField(m_SurvivalMysteryBoxSprites);
        EditorGUILayout.PropertyField(m_SurvivalStackableSprites);
        //EditorGUILayout.PropertyField(m_SurvivalDefaultItems);

        serializedObject.ApplyModifiedProperties();
    }

}
