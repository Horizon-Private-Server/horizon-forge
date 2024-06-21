using System;
using System.Collections.Generic;
using System.Linq;
using JetBrains.Annotations;
using UnityEngine;

public static class HierarchicalSorting
{
    private static int Compare([CanBeNull] Component x, [CanBeNull] Component y)
    {
        return Compare(x != null ? x.transform : null, y != null ? y.transform : null);
    }

    private static int Compare([CanBeNull] GameObject x, [CanBeNull] GameObject y)
    {
        return Compare(x != null ? x.transform : null, y != null ? y.transform : null);
    }

    private static int Compare([CanBeNull] Transform x, [CanBeNull] Transform y)
    {
        if (x == null && y == null)
            return 0;

        if (x == null)
            return -1;

        if (y == null)
            return +1;

        var hierarchy1 = GetHierarchy(x);
        var hierarchy2 = GetHierarchy(y);

        while (true)
        {
            if (!hierarchy1.Any())
                return -1;

            var pop1 = hierarchy1.Pop();

            if (!hierarchy2.Any())
                return +1;

            var pop2 = hierarchy2.Pop();

            var compare = pop1.CompareTo(pop2);

            if (compare == 0)
                continue;

            return compare;
        }
    }

    [NotNull]
    private static Stack<int> GetHierarchy([NotNull] Transform transform)
    {
        if (transform == null)
            throw new ArgumentNullException(nameof(transform));

        var stack = new Stack<int>();

        var current = transform;

        while (current != null)
        {
            var index = current.GetSiblingIndex();

            stack.Push(index);

            current = current.parent;
        }

        return stack;
    }

    [PublicAPI]
    [NotNull]
    [ItemNotNull]
    public static T[] Sort<T>([NotNull][ItemNotNull] T[] components) where T : Component
    {
        if (components == null)
            throw new ArgumentNullException(nameof(components));

        Array.Sort(components, new RelayComparer<T>(Compare));

        return components;
    }

    [PublicAPI]
    [NotNull]
    [ItemNotNull]
    public static GameObject[] Sort([NotNull][ItemNotNull] GameObject[] gameObjects)
    {
        if (gameObjects == null)
            throw new ArgumentNullException(nameof(gameObjects));

        Array.Sort(gameObjects, new RelayComparer<GameObject>(Compare));

        return gameObjects;
    }

    [PublicAPI]
    [NotNull]
    [ItemNotNull]
    public static Transform[] Sort([NotNull][ItemNotNull] Transform[] transforms)
    {
        if (transforms == null)
            throw new ArgumentNullException(nameof(transforms));

        Array.Sort(transforms, new RelayComparer<Transform>(Compare));

        return transforms;
    }

    private sealed class RelayComparer<T> : Comparer<T>
    {
        public RelayComparer([NotNull] Func<T, T, int> func)
        {
            Func = func ?? throw new ArgumentNullException(nameof(func));
        }

        [NotNull]
        private Func<T, T, int> Func { get; }

        public override int Compare(T x, T y)
        {
            return Func(x, y);
        }
    }
}
