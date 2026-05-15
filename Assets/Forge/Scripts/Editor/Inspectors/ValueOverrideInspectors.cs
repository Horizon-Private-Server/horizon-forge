using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

// default inspectors for value overrides
// recommended to use a custom inspector with UnityHelper.FloatOverride
// so that the default value can be passed in

[CustomPropertyDrawer(typeof(FloatOverride), true)]
public class FloatOverrideDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return EditorGUIUtility.singleLineHeight;
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        var noDefaultAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(OverrideNoDefaultAttribute)) as OverrideNoDefaultAttribute;
		if (noDefaultAttr is not null && property.FindPropertyRelative("HasOverride").boolValue == false)
		{
			UnityHelper.EmptyOverride(rect, property, null);
			return;
		}

        var defaultValue = 0f;
        var defaultValueAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(DefaultValueAttribute)) as DefaultValueAttribute;
        if (defaultValueAttr is not null)
            defaultValue = defaultValueAttr.Value.TryConvertToSingle() ?? 0f;

        UnityHelper.FloatOverride(rect, property, null, defaultValue);
    }
}

[CustomPropertyDrawer(typeof(DoubleOverride), true)]
public class DoubleOverrideDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return EditorGUIUtility.singleLineHeight;
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        var noDefaultAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(OverrideNoDefaultAttribute)) as OverrideNoDefaultAttribute;
		if (noDefaultAttr is not null && property.FindPropertyRelative("HasOverride").boolValue == false)
		{
			UnityHelper.EmptyOverride(rect, property, null);
			return;
		}

        var defaultValue = 0d;
        var defaultValueAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(DefaultValueAttribute)) as DefaultValueAttribute;
        if (defaultValueAttr is not null)
            defaultValue = defaultValueAttr.Value.TryConvertToDouble() ?? 0f;

        UnityHelper.DoubleOverride(rect, property, null, defaultValue);
    }
}

[CustomPropertyDrawer(typeof(BoolOverride), true)]
public class BoolOverrideDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return EditorGUIUtility.singleLineHeight;
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        var noDefaultAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(OverrideNoDefaultAttribute)) as OverrideNoDefaultAttribute;
		if (noDefaultAttr is not null && property.FindPropertyRelative("HasOverride").boolValue == false)
		{
			UnityHelper.EmptyOverride(rect, property, null);
			return;
		}

        var defaultValue = false;
        var defaultValueAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(DefaultValueAttribute)) as DefaultValueAttribute;
        if (defaultValueAttr is not null)
            defaultValue = defaultValueAttr.Value.TryConvertToBool() ?? false;

        UnityHelper.BoolOverride(rect, property, null, defaultValue);
    }
}

[CustomPropertyDrawer(typeof(Int32Override), true)]
public class Int32OverrideDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return EditorGUIUtility.singleLineHeight;
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        var noDefaultAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(OverrideNoDefaultAttribute)) as OverrideNoDefaultAttribute;
		if (noDefaultAttr is not null && property.FindPropertyRelative("HasOverride").boolValue == false)
		{
			UnityHelper.EmptyOverride(rect, property, null);
			return;
		}

        var defaultValue = 0;
        var defaultValueAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(DefaultValueAttribute)) as DefaultValueAttribute;
        if (defaultValueAttr is not null)
            defaultValue = defaultValueAttr.Value.TryConvertToInt32() ?? 0;

        var rangeAttr = fieldInfo.GetCustomAttributes(typeof(OverrideRangeAttribute), true).FirstOrDefault() as OverrideRangeAttribute;
        UnityHelper.Int32Override(rect, property, null, defaultValue, rangeMin: rangeAttr?.min, rangeMax: rangeAttr?.max);
    }
}

[CustomPropertyDrawer(typeof(UInt32Override), true)]
public class UInt32OverrideDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return EditorGUIUtility.singleLineHeight;
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        var noDefaultAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(OverrideNoDefaultAttribute)) as OverrideNoDefaultAttribute;
		if (noDefaultAttr is not null && property.FindPropertyRelative("HasOverride").boolValue == false)
		{
			UnityHelper.EmptyOverride(rect, property, null);
			return;
		}

        uint defaultValue = 0;
        var defaultValueAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(DefaultValueAttribute)) as DefaultValueAttribute;
        if (defaultValueAttr is not null)
            defaultValue = defaultValueAttr.Value.TryConvertToUInt32() ?? 0;

        UnityHelper.UInt32Override(rect, property, null, defaultValue);
    }
}

[CustomPropertyDrawer(typeof(EnumOverride<>), true)]
public class EnumOverrideDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return EditorGUIUtility.singleLineHeight;
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        var noDefaultAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(OverrideNoDefaultAttribute)) as OverrideNoDefaultAttribute;
		if (noDefaultAttr is not null && property.FindPropertyRelative("HasOverride").boolValue == false)
		{
			UnityHelper.EmptyOverride(rect, property, null);
			return;
		}

        Type genericType = fieldInfo.FieldType;
        if (genericType.IsGenericType && genericType.GetGenericTypeDefinition() == typeof(EnumOverride<>))
        {
            Type enumType = genericType.GetGenericArguments()[0];
			var flagsAttr = enumType.GetCustomAttributes(typeof(FlagsAttribute), true).FirstOrDefault() as FlagsAttribute;

            // get default value
			// use first value in enum as fallback
            Enum defaultValue = flagsAttr is not null ? (Enum)Enum.ToObject(enumType, 0) : (Enum)Enum.GetValues(enumType).GetValue(0);
            var defaultValueAttr = fieldInfo.GetCustomAttributes(typeof(DefaultValueAttribute), true).FirstOrDefault() as DefaultValueAttribute;
            if (defaultValueAttr != null && defaultValueAttr.Value != null)
                defaultValue = (Enum)Enum.ToObject(enumType, defaultValueAttr.Value);

            UnityHelper.EnumOverride(rect, property, null, defaultValue);
        }
        else
        {
            EditorGUI.PropertyField(rect, property, label);
        }
    }
}
