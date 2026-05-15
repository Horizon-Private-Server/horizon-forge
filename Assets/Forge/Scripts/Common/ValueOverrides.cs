using System;
using System.Collections.Generic;
using System.Text;
using UnityEngine;


[Serializable]
public struct FloatOverride
{
    public bool HasOverride;
    public float OverrideValue;

    public float GetValue(float defaultValue) => HasOverride ? OverrideValue : defaultValue;
    public float? GetValue() => HasOverride ? OverrideValue : null;
}

[Serializable]
public struct DoubleOverride
{
    public bool HasOverride;
    public double OverrideValue;

    public double GetValue(double defaultValue) => HasOverride ? OverrideValue : defaultValue;
    public double? GetValue() => HasOverride ? OverrideValue : null;
}

[Serializable]
public struct BoolOverride
{
    public bool HasOverride;
    public bool OverrideValue;

    public bool GetValue(bool defaultValue) => HasOverride ? OverrideValue : defaultValue;
    public bool? GetValue() => HasOverride ? OverrideValue : null;
}

[Serializable]
public struct UInt32Override
{
    public bool HasOverride;
    public uint OverrideValue;

    public uint GetValue(uint defaultValue) => HasOverride ? OverrideValue : defaultValue;
    public uint? GetValue() => HasOverride ? OverrideValue : null;
}

[Serializable]
public struct Int32Override
{
    public bool HasOverride;
    public int OverrideValue;

    public int GetValue(int defaultValue) => HasOverride ? OverrideValue : defaultValue;
    public int? GetValue() => HasOverride ? OverrideValue : null;
}

[Serializable]
public struct ColorNoAlphaOverride
{
    public bool HasOverride;
    [ColorUsage(false)] public Color OverrideValue;

    public Color GetValue(Color defaultValue) => HasOverride ? OverrideValue : defaultValue;
    public Color? GetValue() => HasOverride ? OverrideValue : null;
}

[Serializable]
public struct EnumOverride<T>
{
    public bool HasOverride;
    public T OverrideValue;

    public T GetValue(T defaultValue) => HasOverride ? OverrideValue : defaultValue;
}

public class OverrideRangeAttribute : PropertyAttribute
{
    public float min;
    public float max;

    public OverrideRangeAttribute(float min, float max)
    {
        this.min = min;
        this.max = max;
    }
}

public class OverrideNoDefaultAttribute : PropertyAttribute
{
    public OverrideNoDefaultAttribute() {}
}
