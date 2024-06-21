using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Unity.VisualScripting;
using UnityEngine;

public static class MathHelper
{
    public static Vector3 SwizzleXZY(this Vector3 v)
    {
        return new Vector3(v.x, v.z, v.y);
    }

    public static Matrix4x4 SwizzleXZY(this Matrix4x4 m)
    {
        var s = new Matrix4x4();

        s[0 + 0] = m[0];
        s[0 + 2] = m[1];
        s[0 + 1] = m[2];
        s[0 + 3] = m[3];
        s[8 + 0] = m[4];
        s[8 + 2] = m[5];
        s[8 + 1] = m[6];
        s[8 + 3] = m[7];
        s[4 + 0] = m[8];
        s[4 + 2] = m[9];
        s[4 + 1] = m[10];
        s[4 + 3] = m[11];
        s[12 + 0] = m[12];
        s[12 + 2] = m[13];
        s[12 + 1] = m[14];
        s[12 + 3] = m[15];

        return s;
    }

    public static Vector3 GetScale(this Matrix4x4 m)
    {
        //if (m.ValidTRS()) return m.lossyScale;
        
        return new Vector3(
            m.GetColumn(0).magnitude,
            m.GetColumn(1).magnitude,
            m.GetColumn(2).magnitude
        );
    }

    public static Quaternion GetRotation(this Matrix4x4 m)
    {
        //if (m.ValidTRS()) return m.rotation;

        return Quaternion.LookRotation(m.GetColumn(2), m.GetColumn(1));
    }

    public static void GetReflectionMatrix(this Matrix4x4 m, out Vector3 position, out Quaternion rotation, out Vector3 scale, out Matrix4x4 reflection)
    {
        position = m.GetPosition();
        rotation = m.GetRotation();
        scale = m.GetScale();
        var trs = Matrix4x4.TRS(position, rotation, scale);
        reflection = m * trs.inverse;

        // check for negative scale in reflection
        if (Vector3.Dot(Vector3.right, reflection.MultiplyVector(Vector3.right)) < 0)
            scale.x *= -1f;
        if (Vector3.Dot(Vector3.up, reflection.MultiplyVector(Vector3.up)) < 0)
            scale.y *= -1f;
        if (Vector3.Dot(Vector3.forward, reflection.MultiplyVector(Vector3.forward)) < 0)
            scale.z *= -1f;

        // recompute rotation factoring in negative scale
        rotation = Quaternion.LookRotation(m.GetColumn(2) * Mathf.Sign(scale.z), m.GetColumn(1) * Mathf.Sign(scale.y));

        // verify final matrix matches original
        trs = Matrix4x4.TRS(position, rotation, scale);
        reflection = m * trs.inverse;
        var m2 = reflection * trs;
        if (Vector3.Dot(m2.MultiplyVector(Vector3.right), m.MultiplyVector(Vector3.right)) < 0)
            scale.x *= -1f;
        if (Vector3.Dot(m2.MultiplyVector(Vector3.up), m.MultiplyVector(Vector3.up)) < 0)
            scale.y *= -1f;
        if (Vector3.Dot(m2.MultiplyVector(Vector3.forward), m.MultiplyVector(Vector3.forward)) < 0)
            scale.z *= -1f;

        // recompute reflection
        trs = Matrix4x4.TRS(position, rotation, scale);
        reflection = m * trs.inverse;
        reflection[15] = 1;

        // check if identity
        if (reflection.isIdentity)
        {
            reflection = Matrix4x4.identity;
            return;
        }

        // sometimes the reflection matrix is slightly off from the identity
        // because of editor limitations with the matrix, it is convenient if we have the identity
        // so force identity in those cases
        var id = Matrix4x4.identity;
        for (int i = 0; i < 16; i++)
            if (Mathf.Abs(reflection[i] - id[i]) > 0.0003f)
                return;

        reflection = id;
    }

    public static Vector3 Average(this IEnumerable<Vector3> vectors)
    {
        if (!vectors.Any()) return Vector3.zero;

        var sum = Vector3.zero;
        var count = vectors.Count();
        foreach (var v in vectors) sum += v / count;

        return sum;
    }
}
