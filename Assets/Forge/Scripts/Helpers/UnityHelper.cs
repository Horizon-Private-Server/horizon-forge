using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.AI;

public static class UnityHelper
{
    private static Texture2D _defaultTexture;
    public static Texture2D DefaultTexture => _defaultTexture ? _defaultTexture : (_defaultTexture = new Texture2D(32, 32, TextureFormat.ARGB32, false));

    public static void Matrix4x4PropertyField(SerializedProperty property)
    {
        EditorGUI.BeginDisabledGroup(!property.editable);
        property.isExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(property.isExpanded, property.displayName);
        if (property.isExpanded)
        {
            GUILayout.BeginVertical();

            for (int y = 0; y < 4; ++y)
            {
                GUILayout.BeginHorizontal();

                for (int x = 0; x < 4; ++x)
                {
                    var prop = property.FindPropertyRelative("e" + y + x);
                    EditorGUILayout.PropertyField(prop, new GUIContent(""));
                }

                GUILayout.EndHorizontal();
            }

            GUILayout.EndVertical();

            // clear
            if (GUILayout.Button("Reset"))
                SetMatrix4x4PropertyField(property, Matrix4x4.identity);
        }
        EditorGUI.EndDisabledGroup();
    }

    public static void SetMatrix4x4PropertyField(SerializedProperty property, Matrix4x4 m)
    {
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                var prop = property.FindPropertyRelative("e" + y + x);
                prop.floatValue = m[x, y];
            }
        }
    }

    public static void RecurseHierarchy(Transform root, Action<Transform> onNode)
    {
        if (!root) return;
        onNode(root);

        foreach (Transform child in root)
            RecurseHierarchy(child, onNode);
    }

    public static void CloneHierarchy(Transform srcRoot, Transform dstRoot, Func<Transform, Transform, bool> onNode)
    {
        if (!srcRoot) return;
        if (!dstRoot) return;

        // copy
        dstRoot.transform.localPosition = srcRoot.transform.localPosition;
        dstRoot.transform.localRotation = srcRoot.transform.localRotation;
        dstRoot.transform.localScale = srcRoot.transform.localScale;

        if (!onNode(srcRoot, dstRoot.transform))
        {
            GameObject.DestroyImmediate(dstRoot.gameObject);
            return;
        }

        foreach (Transform child in srcRoot)
        {
            var newNode = new GameObject(child.gameObject.name);
            newNode.transform.SetParent(dstRoot.transform, false);
            CloneHierarchy(child, newNode.transform, onNode);
        }
    }

    public static Transform FindInHierarchy(Transform root, string childName)
    {
        if (root.name == childName)
            return root;

        for (int i = 0; i < root.childCount; ++i)
        {
            var hit = FindInHierarchy(root.GetChild(i), childName);
            if (hit)
                return hit;
        }

        return null;
    }

    public static string GetPath(Transform root, Transform t)
    {
        if (root == t)
            return "";

        return (GetPath(root, t.parent) + "/" + t.name).TrimStart('/');
    }

    public static GameObject GetAssetPrefab(string assetType, string oClass, bool includeGlobal = false)
    {
        // always return local asset path
        var path = FolderNames.GetLocalAssetFolder(assetType);
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}.fbx"));

        if (includeGlobal && !prefab)
        {
            path = FolderNames.GetGlobalAssetFolder(assetType);
            prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}.fbx"));
        }

        return prefab;
    }

    public static GameObject GetAssetColliderPrefab(string assetType, string oClass, bool includeGlobal = false)
    {
        // always return local asset path
        var path = FolderNames.GetLocalAssetFolder(assetType);
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.fbx"));
        if (!prefab) prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.blend"));

        if (includeGlobal && !prefab)
        {
            path = FolderNames.GetGlobalAssetFolder(assetType);
            prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.fbx"));
            if (!prefab) prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.blend"));
        }

        return prefab;
    }

    public static GameObject GetCuboidPrefab(CuboidType type, CuboidSubType subtype)
    {
        var path = FolderNames.GetGlobalPrefabFolder("Cuboid");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{type}.prefab"));
        if (subtype != CuboidSubType.Default && (type == CuboidType.Player || type == CuboidType.None))
            prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{subtype}.prefab"));

        return prefab;
    }

    public static GameObject GetSNDPrefab(string prefabName)
    {
        var path = FolderNames.GetGlobalPrefabFolder("SND");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{prefabName}.prefab"));
        return prefab;
    }

    public static GameObject GetMiscPrefab(string prefabName)
    {
        var path = FolderNames.GetGlobalPrefabFolder("Misc");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{prefabName}.prefab"));
        return prefab;
    }

    public static string GetProjectRelativePath(string absolutePath) => Path.GetRelativePath(Environment.CurrentDirectory, absolutePath);

    public static void ImportTexture(string path)
    {
        var assetPath = UnityHelper.GetProjectRelativePath(path);
        AssetDatabase.ImportAsset(assetPath);
        TextureImporter importer = (TextureImporter)TextureImporter.GetAtPath(assetPath);
        importer.alphaIsTransparency = true;
        importer.SaveAndReimport();
    }

    public static List<IOcclusionData> GetAllOcclusionDataInSelection()
    {
        return Selection.gameObjects?.SelectMany(x => x.GetComponentsInChildren<IOcclusionData>())?.ToList();
    }

    public static List<Vector3> GetAllOctants()
    {
        var volumes = GameObject.FindObjectsOfType<OcclusionVolume>();
        var rawOctants = GameObject.FindObjectsOfType<OcclusionOctant>();
        var octants = volumes.Where(x => !x.Negate).SelectMany(x => x.GetOctants()).Union(rawOctants.SelectMany(x => x.Octants ?? new List<Vector3>())).Distinct().ToList();
        var negativeOctants = volumes.Where(x => x.Negate).ToList();
        octants.RemoveAll(x => negativeOctants.Any(o => o.Contains(x)));

        return octants;
    }

    public static void DrawLine(Vector3 from, Vector3 to, Color color, float thickness)
    {
        Handles.DrawBezier(from, to, from, to, color, null, thickness);
    }

    public static void SaveRenderTexture(RenderTexture rt, string path)
    {
        Texture2D tex = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false);
        RenderTexture.active = rt;
        tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        tex.Apply();

        byte[] bytes = tex.EncodeToPNG();
        System.IO.File.WriteAllBytes(path, bytes);
        AssetDatabase.ImportAsset(path);
    }

    public static bool SaveTexture(Texture2D tex, string path, bool forcePowerOfTwo = false, int? maxTexSize = null)
    {
        if (tex)
        {
            // copy file if no operations need be done on the texture
            var assetPath = AssetDatabase.GetAssetPath(tex);
            if (!String.IsNullOrEmpty(assetPath) && Path.GetExtension(assetPath) == ".png")
            {
                if (maxTexSize == null || (tex.width <= maxTexSize && tex.height <= maxTexSize))
                {
                    if (!forcePowerOfTwo || (Mathf.Log(tex.width, 2) == tex.width && Mathf.Log(tex.height, 2) == tex.height))
                    {
                        File.Copy(assetPath, path, true);
                        return true;
                    }
                }
            }

            var width = tex.width;
            var height = tex.height;
            if (forcePowerOfTwo)
            {
                if (width > height && width > maxTexSize)
                {
                    height = Mathf.CeilToInt(height * (maxTexSize.Value / (float)width));
                    width = maxTexSize.Value;
                }
                else if (height > width && height > maxTexSize)
                {
                    width = Mathf.CeilToInt(width * (maxTexSize.Value / (float)height));
                    height = maxTexSize.Value;
                }
                else if (width > maxTexSize)
                {
                    width = maxTexSize.Value;
                    height = maxTexSize.Value;
                }

                // force power of two
                width = ForceDimensionPowerOfTwo(width);
                height = ForceDimensionPowerOfTwo(height);
            }

            var rt = new RenderTexture(width, height, 0, RenderTextureFormat.ARGB32);
            rt.Create();
            try
            {
                var mat = new Material(AssetDatabase.LoadAssetAtPath<Material>(Path.Combine(FolderNames.ForgeFolder, "Shaders", "TintBlit.mat")));
                mat.SetColor("_Color", Color.white);
                mat.SetTexture("_In", tex);
                mat.SetTexture("_Out", rt);
                Graphics.Blit(tex, rt, mat);

                var oldRt = RenderTexture.active;
                RenderTexture.active = rt;
                var tex2 = new Texture2D(width, height, TextureFormat.ARGB32, false);
                tex2.ReadPixels(new Rect(0, 0, width, height), 0, 0);
                tex2.Apply();
                RenderTexture.active = oldRt;

                var bytes = tex2.EncodeToPNG();
                File.WriteAllBytes(path, bytes);
                return true;
            }
            finally
            {
                if (RenderTexture.active == rt)
                    RenderTexture.active = null;

                rt.Release();
            }
        }

        return false;
    }

    public static Texture2D ResizeTexture(Texture2D src, int width, int height)
    {
        if (!src) return null;

        RenderTexture rt = new RenderTexture(width, height, 24);
        RenderTexture.active = rt;
        Graphics.Blit(src, rt);
        Texture2D result = new Texture2D(width, height);
        result.ReadPixels(new Rect(0, 0, width, height), 0, 0);
        result.Apply();
        rt.Release();
        return result;
    }

    public static Color32 GetColor(this uint rgba)
    {
        return new Color32(
            (byte)((rgba >> 0) & 0xff),
            (byte)((rgba >> 8) & 0xff),
            (byte)((rgba >> 16) & 0xff),
            (byte)((rgba >> 24) & 0xff)
            );
    }

    public static Color HalveRGB(this Color color)
    {
        return new Color(color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, color.a);
    }

    public static Color DoubleRGB(this Color color)
    {
        return new Color(color.r * 2f, color.g * 2f, color.b * 2f, color.a);
    }

    static int ForceDimensionPowerOfTwo(int dimension)
    {
        float exp = Mathf.Log(dimension, 2);
        if (exp == (int)exp) return dimension;

        return (int)Mathf.Pow(2, Mathf.CeilToInt(exp));
    }

    public static int ComputeHash(this Mesh mesh)
    {
        int hash = 0;
        if (!mesh) return 0;

        mesh.GetHashCode();
        foreach (var v in mesh.vertices)
            hash = hash ^ v.GetHashCode();

        return hash;
    }

    public static void FlipFaces(this Mesh mesh, int subMeshIndex = -1)
    {
        for (int i = 0; i < mesh.subMeshCount; ++i)
        {
            if (subMeshIndex != -1 && i != subMeshIndex) continue;

            var triangles = mesh.GetTriangles(i);
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var t = triangles[j + 2];
                triangles[j + 2] = triangles[j];
                triangles[j] = t;
            }
            mesh.SetTriangles(triangles, i);
        }
    }

    public static void AddBackSideFaces(this Mesh mesh)
    {
        var subMeshCount = mesh.subMeshCount;
        mesh.subMeshCount *= 2;
        for (int i = 0; i < subMeshCount; ++i)
        {
            var triangles = mesh.GetTriangles(i);
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var t = triangles[j + 2];
                triangles[j + 2] = triangles[j];
                triangles[j] = t;
            }
            mesh.SetTriangles(triangles, subMeshCount + i);
        }
    }

    public static void RecalculateFaceNormals(this Mesh mesh, float normalFactor = 1f, bool flip = false)
    {
        var flipNormal = flip ? -1 : 1;
        for (int i = 0; i < mesh.subMeshCount; ++i)
        {
            var triangles = mesh.GetTriangles(i);

            // get center
            var centerSum = Vector3.zero;
            var centerCount = 0f;
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var faceCenter = (mesh.vertices[triangles[j + 0]] + mesh.vertices[triangles[j + 1]] + mesh.vertices[triangles[j + 2]]) / 3;
                var normal = Vector3.Cross(mesh.vertices[triangles[j + 1]] - mesh.vertices[triangles[j + 0]], mesh.vertices[triangles[j + 2]] - mesh.vertices[triangles[j + 0]]);

                centerSum += (faceCenter + normal * normalFactor);
                centerCount += 1;

                //centerSum += mesh.vertices[triangles[j + 0]];
                //centerSum += mesh.vertices[triangles[j + 1]];
                //centerSum += mesh.vertices[triangles[j + 2]];
                //centerCount += 3;
            }

            var center = (centerSum / centerCount);
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var faceCenter = (mesh.vertices[triangles[j + 0]] + mesh.vertices[triangles[j + 1]] + mesh.vertices[triangles[j + 2]]) / 3;
                var normal = flipNormal * Vector3.Cross(mesh.vertices[triangles[j + 1]] - mesh.vertices[triangles[j + 0]], mesh.vertices[triangles[j + 2]] - mesh.vertices[triangles[j + 0]]);
                if (Vector3.Dot(normal, faceCenter - center) < 0)
                {
                    var t = triangles[j + 2];
                    triangles[j + 2] = triangles[j];
                    triangles[j] = t;
                }
            }
            mesh.SetTriangles(triangles, i);
        }
    }
}
