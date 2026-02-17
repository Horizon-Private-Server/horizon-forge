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
}

[Serializable]
public struct DoubleOverride
{
    public bool HasOverride;
    public double OverrideValue;

    public double GetValue(double defaultValue) => HasOverride ? OverrideValue : defaultValue;
}

[Serializable]
public struct BoolOverride
{
    public bool HasOverride;
    public bool OverrideValue;

    public bool GetValue(bool defaultValue) => HasOverride ? OverrideValue : defaultValue;
}

[Serializable]
public struct UInt32Override
{
    public bool HasOverride;
    public uint OverrideValue;

    public uint GetValue(uint defaultValue) => HasOverride ? OverrideValue : defaultValue;
}

[Serializable]
public struct Int32Override
{
    public bool HasOverride;
    public int OverrideValue;

    public int GetValue(int defaultValue) => HasOverride ? OverrideValue : defaultValue;
}

[Serializable]
public struct ColorNoAlphaOverride
{
    public bool HasOverride;
    [ColorUsage(false)] public Color OverrideValue;

    public Color GetValue(Color defaultValue) => HasOverride ? OverrideValue : defaultValue;
}

[Serializable]
public struct EnumOverride<T>
{
    public bool HasOverride;
    public T OverrideValue;

    public T GetValue(T defaultValue) => HasOverride ? OverrideValue : defaultValue;
}
