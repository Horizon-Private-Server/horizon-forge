using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

[CustomPropertyDrawer(typeof(RaidsMobSpawnParam))]
public class RaidsMobSpawnParamPropertyDrawer : PropertyDrawer
{
    const float LINE_HEIGHT = 18f;

    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return 20f + LINE_HEIGHT * (property.isExpanded ? 35 : 0);
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        var boldLabel = new GUIStyle(GUI.skin.label);
        boldLabel.fontStyle = FontStyle.Bold;

        var nameProperty = property.FindPropertyRelative("Name");
        var disabledProperty = property.FindPropertyRelative("Disabled");
        var mobProperty = property.FindPropertyRelative("Mob");
        var variantProperty = property.FindPropertyRelative("Variant");
        var texturePaletteProperty = property.FindPropertyRelative("TexturePalette");

        var sizeProperty = property.FindPropertyRelative("SizeMultiplier");
        var xpProperty = property.FindPropertyRelative("XpMultiplier");
        var boltProperty = property.FindPropertyRelative("BoltsMultiplier");

        var damageProperty = property.FindPropertyRelative("DamageMultiplier");
        var damageScaleProperty = property.FindPropertyRelative("DamageDifficultyRateMultiplier");
        var speedProperty = property.FindPropertyRelative("SpeedMultiplier");
        var speedScaleProperty = property.FindPropertyRelative("SpeedDifficultyRateMultiplier");
        var healthProperty = property.FindPropertyRelative("HealthMultiplier");
        var healthScaleProperty = property.FindPropertyRelative("HealthDifficultyRateMultiplier");

        var turnSpeedProperty = property.FindPropertyRelative("TurnSpeedMultiplier");
        var rangedAttackDistProperty = property.FindPropertyRelative("RangedAttackDistance");
        var visionRangeProperty = property.FindPropertyRelative("VisionRange");
        var peripheralVisionDegreesProperty = property.FindPropertyRelative("PeripheralVisionDegrees");
        var forceAggroRangeProperty = property.FindPropertyRelative("ForceAggroRange");
        var outOfSightDeAggroTimeProperty = property.FindPropertyRelative("OutOfSightDeAggroTime");

        var lineRect = Rect.MinMaxRect(rect.xMin, rect.yMin, rect.xMax, rect.yMin + LINE_HEIGHT);
        property.isExpanded = EditorGUI.BeginFoldoutHeaderGroup(lineRect, property.isExpanded, label); lineRect.y += 20;
        if (property.isExpanded)
        {
            EditorGUI.PropertyField(lineRect, nameProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, disabledProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, mobProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, variantProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, texturePaletteProperty); lineRect.y += LINE_HEIGHT;

            lineRect.y += LINE_HEIGHT;
            EditorGUI.LabelField(lineRect, "General", boldLabel); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, sizeProperty); lineRect.y += LINE_HEIGHT;

            lineRect.y += LINE_HEIGHT;
            EditorGUI.LabelField(lineRect, "Stats", boldLabel); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, xpProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, boltProperty); lineRect.y += LINE_HEIGHT;

            lineRect.y += LINE_HEIGHT;
            EditorGUI.LabelField(lineRect, "Damage", boldLabel); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, damageProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, damageScaleProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.HelpBox(lineRect, GetScaleText(property, "Damage"), MessageType.Info); lineRect.y += LINE_HEIGHT;

            lineRect.y += LINE_HEIGHT;
            EditorGUI.LabelField(lineRect, "Speed", boldLabel); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, speedProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, speedScaleProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.HelpBox(lineRect, GetScaleText(property, "Speed"), MessageType.Info); lineRect.y += LINE_HEIGHT;

            lineRect.y += LINE_HEIGHT;
            EditorGUI.LabelField(lineRect, "Health", boldLabel); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, healthProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, healthScaleProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.HelpBox(lineRect, GetScaleText(property, "Health"), MessageType.Info); lineRect.y += LINE_HEIGHT;


            lineRect.y += LINE_HEIGHT;
            EditorGUI.LabelField(lineRect, "Interaction", boldLabel); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, turnSpeedProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, rangedAttackDistProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, visionRangeProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, peripheralVisionDegreesProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, forceAggroRangeProperty); lineRect.y += LINE_HEIGHT;
            EditorGUI.PropertyField(lineRect, outOfSightDeAggroTimeProperty); lineRect.y += LINE_HEIGHT;

        }
        EditorGUI.EndFoldoutHeaderGroup();
    }

    string GetScaleText(SerializedProperty property, string field)
    {
        var baseProperty = property.FindPropertyRelative($"{field}Multiplier");
        var scaleProperty = property.FindPropertyRelative($"{field}DifficultyRateMultiplier");
        var mobProperty = property.FindPropertyRelative("Mob");
        var mobConfig = RaidsMobsScriptableObject.Load();
        var mobData = mobConfig.Mobs.FirstOrDefault(x => x.Mob == (RaidsMob)mobProperty.enumValueIndex);
        var max = mobData.HealthMax;
        var init = baseProperty.floatValue;
        var scale = scaleProperty.floatValue;
        Func<float, float, float, int, float> get = null;
        
        switch (field)
        {
            case "Damage":
                {
                    max = mobData.DamageMax;
                    init *= mobData.Damage;
                    scale *= mobData.DamageScale;
                    get = RaidsModeData.ScaleDamage;
                    break;
                }
            case "Speed":
                {
                    max = mobData.SpeedMax;
                    init *= mobData.Speed;
                    scale *= mobData.SpeedScale;
                    get = RaidsModeData.ScaleSpeed;
                    break;
                }
            case "Health":
                {
                    max = mobData.HealthMax;
                    init *= mobData.Health;
                    scale *= mobData.HealthScale;
                    get = RaidsModeData.ScaleHealth;
                    break;
                }
        }

        return $"1 Star: {get(init, max, scale, 0):N}, "
            + $"2 Star: {get(init, max, scale, 1):N}, "
            + $"3 Star: {get(init, max, scale, 2):N}, "
            + $"4 Star: {get(init, max, scale, 3):N}, "
            + $"5 Star: {get(init, max, scale, 4):N}"
            ;
    }

}
