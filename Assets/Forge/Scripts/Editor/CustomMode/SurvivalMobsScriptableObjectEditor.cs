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

    private void OnEnable()
    {
        m_MobsProperty = serializedObject.FindProperty("Mobs");
    }


    public override void OnInspectorGUI()
    {
        var manager = (target as SurvivalMobsScriptableObject);
        //base.OnInspectorGUI();

        serializedObject.Update();

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

        serializedObject.ApplyModifiedProperties();
    }

}
