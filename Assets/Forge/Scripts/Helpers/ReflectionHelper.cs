using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Reflection;
using System.Text;
using UnityEngine;

public static class ReflectionHelper
{
    public static string GetInspectorName(this Enum value)
    {
        if (value == null)
            return string.Empty;

        var type = value.GetType();
        var name = value.ToString();

        var field = type.GetField(name);
        if (field == null)
            return name;

        var attribute = field.GetCustomAttribute<InspectorNameAttribute>();
        return attribute?.displayName ?? name;
    }

    public static string GetDescription(this Enum value)
    {
        if (value == null)
            return string.Empty;

        var type = value.GetType();
        var name = value.ToString();

        var field = type.GetField(name);
        if (field == null)
            return name;

        var attribute = field.GetCustomAttribute<DescriptionAttribute>();
        return attribute?.Description ?? name;
    }

    public static string GetTooltip(this Enum value)
    {
        if (value == null)
            return string.Empty;

        var type = value.GetType();
        var name = value.ToString();

        var field = type.GetField(name);
        if (field == null)
            return name;

        var attribute = field.GetCustomAttribute<TooltipAttribute>();
        return attribute?.tooltip ?? name;
    }

    public static float? TryConvertToSingle(this object value)
    {
        try
        {
            return Convert.ToSingle(value);
        }
        catch
        {
            return null;
        }
    }

    public static double? TryConvertToDouble(this object value)
    {
        try
        {
            return Convert.ToDouble(value);
        }
        catch
        {
            return null;
        }
    }

    public static int? TryConvertToInt32(this object value)
    {
        try
        {
            return Convert.ToInt32(value);
        }
        catch
        {
            return null;
        }
    }

    public static uint? TryConvertToUInt32(this object value)
    {
        try
        {
            return Convert.ToUInt32(value);
        }
        catch
        {
            return null;
        }
    }

    public static bool? TryConvertToBool(this object value)
    {
        try
        {
            return Convert.ToBoolean(value);
        }
        catch
        {
            return null;
        }
    }
}
