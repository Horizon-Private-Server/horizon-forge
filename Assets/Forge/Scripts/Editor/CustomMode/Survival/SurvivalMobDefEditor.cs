using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(SurvivalMobDef))]
public class SurvivalMobDefEditor : Editor
{
    private SurvivalMobsScriptableObject m_SurvivalData;

    private SerializedProperty m_MobProperty;
    private SerializedProperty m_VariantProperty;
    private SerializedProperty m_BehaviorProperty;
    private SerializedProperty m_AttributesProperty;
    private SerializedProperty m_BlipTypeProperty;

    private SerializedProperty m_SpecialRoundOnlyProperty;
    private SerializedProperty m_MinRoundProperty;
    private SerializedProperty m_MaxSpawnedAtOnceProperty;
    private SerializedProperty m_MaxSpawnedPerRoundProperty;
    private SerializedProperty m_ProbabilityProperty;
    private SerializedProperty m_SpawnTypeProperty;
    private SerializedProperty m_CooldownTicksProperty;
    private SerializedProperty m_CooldownOffsetPerRoundFactorProperty;

    private SerializedProperty m_SizeMultiplierProperty;
    private SerializedProperty m_TurnSpeedMultiplierProperty;
    private SerializedProperty m_RangedAttackDistanceProperty;
    private SerializedProperty m_XpProperty;
    private SerializedProperty m_BoltsProperty;

    private SerializedProperty m_DamageProperty;
    private SerializedProperty m_DamageMaxProperty;
    private SerializedProperty m_DamageScaleProperty;

    private SerializedProperty m_SpeedProperty;
    private SerializedProperty m_SpeedMaxProperty;
    private SerializedProperty m_SpeedScaleProperty;

    private SerializedProperty m_HealthProperty;
    private SerializedProperty m_HealthMaxProperty;
    private SerializedProperty m_HealthScaleProperty;

    private SerializedProperty m_BaseColorProperty;
    private SerializedProperty m_GlowColorProperty;
    private SerializedProperty m_SpriteColorProperty;

    private void OnEnable()
    {
        m_MobProperty = serializedObject.FindProperty("Mob");
        m_VariantProperty = serializedObject.FindProperty("Variant");
        m_BehaviorProperty = serializedObject.FindProperty("Behavior");
        m_AttributesProperty = serializedObject.FindProperty("Attributes");
        m_BlipTypeProperty = serializedObject.FindProperty("BlipType");

        m_SpecialRoundOnlyProperty = serializedObject.FindProperty("SpecialRoundOnly");
        m_MinRoundProperty = serializedObject.FindProperty("MinRound");
        m_MaxSpawnedAtOnceProperty = serializedObject.FindProperty("MaxSpawnedAtOnce");
        m_MaxSpawnedPerRoundProperty = serializedObject.FindProperty("MaxSpawnedPerRound");
        m_ProbabilityProperty = serializedObject.FindProperty("Probability");
        m_SpawnTypeProperty = serializedObject.FindProperty("SpawnType");
        m_CooldownTicksProperty = serializedObject.FindProperty("CooldownTicks");
        m_CooldownOffsetPerRoundFactorProperty = serializedObject.FindProperty("CooldownOffsetPerRoundFactor");

        m_SizeMultiplierProperty = serializedObject.FindProperty("SizeMultiplier");
        m_TurnSpeedMultiplierProperty = serializedObject.FindProperty("TurnSpeedMultiplier");
        m_RangedAttackDistanceProperty = serializedObject.FindProperty("RangedAttackDistance");
        m_XpProperty = serializedObject.FindProperty("Xp");
        m_BoltsProperty = serializedObject.FindProperty("Bolts");

        m_DamageProperty = serializedObject.FindProperty("Damage");
        m_DamageMaxProperty = serializedObject.FindProperty("DamageMax");
        m_DamageScaleProperty = serializedObject.FindProperty("DamageScale");

        m_SpeedProperty = serializedObject.FindProperty("Speed");
        m_SpeedMaxProperty = serializedObject.FindProperty("SpeedMax");
        m_SpeedScaleProperty = serializedObject.FindProperty("SpeedScale");

        m_HealthProperty = serializedObject.FindProperty("Health");
        m_HealthMaxProperty = serializedObject.FindProperty("HealthMax");
        m_HealthScaleProperty = serializedObject.FindProperty("HealthScale");

        m_BaseColorProperty = serializedObject.FindProperty("BaseColor");
        m_GlowColorProperty = serializedObject.FindProperty("GlowColor");
        m_SpriteColorProperty = serializedObject.FindProperty("SpriteColor");
    }

    public override void OnInspectorGUI()
    {
        if (!m_SurvivalData) m_SurvivalData = SurvivalMobsScriptableObject.Load();
        serializedObject.Update();

        EditorGUILayout.PropertyField(m_MobProperty);

        var mob = ((SurvivalMob[])Enum.GetValues(typeof(SurvivalMob)))[m_MobProperty.enumValueIndex];
        var mobData = m_SurvivalData.Mobs.FirstOrDefault(x => x.Mob == mob);
		if (mobData == null)
			return;

		if (!string.IsNullOrEmpty(mobData.Description))
			EditorGUILayout.HelpBox(mobData.Description, MessageType.Info);

		UnityHelper.PopupField(m_VariantProperty, null, mobData.Variants.Select(x => x.Name).ToArray());
		var variantDescriptionText = mobData.Variants.ElementAtOrDefault(m_VariantProperty.intValue)?.Description;
		if (!string.IsNullOrEmpty(variantDescriptionText))
			EditorGUILayout.HelpBox(variantDescriptionText, MessageType.Info);

		UnityHelper.PopupField(m_BehaviorProperty, null, mobData.Behaviors.Select(x => x.Name).ToArray());
		var behaviorDescriptionText = mobData.Behaviors.ElementAtOrDefault(m_BehaviorProperty.intValue)?.Description;
		if (!string.IsNullOrEmpty(behaviorDescriptionText))
			EditorGUILayout.HelpBox(behaviorDescriptionText, MessageType.Info);

        EditorGUILayout.PropertyField(m_AttributesProperty);
		UnityHelper.EnumOverride(m_BlipTypeProperty, null, mobData.BlipType);

        EditorGUILayout.Space();
        // EditorGUILayout.LabelField("Spawn Parameters", EditorStyles.boldLabel);
        EditorGUILayout.PropertyField(m_SpecialRoundOnlyProperty);
        EditorGUILayout.PropertyField(m_MinRoundProperty);
        EditorGUILayout.PropertyField(m_MaxSpawnedAtOnceProperty);
        EditorGUILayout.PropertyField(m_MaxSpawnedPerRoundProperty);
        EditorGUILayout.PropertyField(m_ProbabilityProperty);
        EditorGUILayout.PropertyField(m_SpawnTypeProperty);
        EditorGUILayout.PropertyField(m_CooldownTicksProperty);
        EditorGUILayout.PropertyField(m_CooldownOffsetPerRoundFactorProperty);

        EditorGUILayout.Space();
        // EditorGUILayout.LabelField("Mob Parameters", EditorStyles.boldLabel);
        EditorGUILayout.PropertyField(m_SizeMultiplierProperty);
        EditorGUILayout.PropertyField(m_TurnSpeedMultiplierProperty);
		UnityHelper.FloatOverride(m_RangedAttackDistanceProperty, null, mobData.RangedAttackDistance);
		UnityHelper.FloatOverride(m_XpProperty, null, mobData.Xp);
		UnityHelper.FloatOverride(m_BoltsProperty, null, mobData.Bolts);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Damage", EditorStyles.boldLabel);
		UnityHelper.FloatOverride(m_DamageProperty, null, mobData.Damage);
		UnityHelper.FloatOverride(m_DamageMaxProperty, null, mobData.DamageMax);
		UnityHelper.FloatOverride(m_DamageScaleProperty, null, mobData.DamageScale);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Speed", EditorStyles.boldLabel);
		UnityHelper.FloatOverride(m_SpeedProperty, null, mobData.Speed);
		UnityHelper.FloatOverride(m_SpeedMaxProperty, null, mobData.SpeedMax);
		UnityHelper.FloatOverride(m_SpeedScaleProperty, null, mobData.SpeedScale);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Health", EditorStyles.boldLabel);
		UnityHelper.FloatOverride(m_HealthProperty, null, mobData.Health);
		UnityHelper.FloatOverride(m_HealthMaxProperty, null, mobData.HealthMax);
		UnityHelper.FloatOverride(m_HealthScaleProperty, null, mobData.HealthScale);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Colors", EditorStyles.boldLabel);
		UnityHelper.ColorOverride(m_BaseColorProperty, null, mobData.BaseColor);
		UnityHelper.ColorOverride(m_GlowColorProperty, null, mobData.GlowColor);
		UnityHelper.ColorOverride(m_SpriteColorProperty, null, mobData.SpriteColor);

        serializedObject.ApplyModifiedProperties();
    }

}
