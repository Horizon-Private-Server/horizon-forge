using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(RaidsMobsScriptableObject))]
public class RaidsMobsScriptableObjectEditor : Editor
{
    private SerializedProperty m_MobsProperty;

    private void OnEnable()
    {
        m_MobsProperty = serializedObject.FindProperty("Mobs");
    }


    public override void OnInspectorGUI()
    {
        var manager = (target as RaidsMobsScriptableObject);
        //base.OnInspectorGUI();

        serializedObject.Update();

        foreach (RaidsMob mob in Enum.GetValues(typeof(RaidsMob)))
        {
            var idx = manager.Mobs.FindIndex(x => x.Mob == mob);
            if (idx < 0)
            {
                manager.Mobs.Add(new RaidsMobsScriptableObject.RaidsMobsConfig()
                {
                    Mob = mob,
                    Variants = new List<RaidsMobsScriptableObject.RaidsMobVariant>()
                    {
                        new RaidsMobsScriptableObject.RaidsMobVariant("Normal", 0, DLMapIds.SP_Battledome, 0)
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
