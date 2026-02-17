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
        var defaultValue = 0;
        var defaultValueAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(DefaultValueAttribute)) as DefaultValueAttribute;
        if (defaultValueAttr is not null)
            defaultValue = defaultValueAttr.Value.TryConvertToInt32() ?? 0;

        UnityHelper.Int32Override(rect, property, null, defaultValue);
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
        uint defaultValue = 0;
        var defaultValueAttr = fieldInfo.GetCustomAttributes(true).FirstOrDefault(x => x.GetType() == typeof(DefaultValueAttribute)) as DefaultValueAttribute;
        if (defaultValueAttr is not null)
            defaultValue = defaultValueAttr.Value.TryConvertToUInt32() ?? 0;

        UnityHelper.UInt32Override(rect, property, null, defaultValue);
    }
}
