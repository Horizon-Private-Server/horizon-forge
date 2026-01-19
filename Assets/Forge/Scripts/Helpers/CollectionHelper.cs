using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public static class CollectionHelper
{
    public static bool CollectionsEqual<T>(IEnumerable<T> a, IEnumerable<T> b)
    {
        if (a == null && b == null) return true;
        if (a == null || b == null) return false;
        if (a.Count() != b.Count()) return false;

        var count = a.Count();
        for (int i = 0; i < count; ++i)
        {
            var aValue = a.ElementAtOrDefault(i);
            var bValue = b.ElementAtOrDefault(i);

            if (aValue == null ^ bValue == null) return false;
            if (!aValue.Equals(bValue)) return false;
        }

        return true;
    }
}
