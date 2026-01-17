using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class ConvertToShrub : MonoBehaviour
{
    public int GroupId = -1;
    public float RenderDistance = 64;
    [ColorUsage(false)] public Color Tint = Color.white * 0.5f;
    [Tooltip("If true and Export Shrubs is enabled, the DZO exporter will automatically include this object.")] public bool DZOExportWithShrubs = true;

    public IEnumerable<ConvertToShrubChild> GetChildren()
    {
        if (!Validate()) return null;

        var children = new List<ConvertToShrubChild>();
        var foundPrefabs = new HashSet<GameObject>();
        var transforms = new List<Transform>();
        transforms.AddRange(this.gameObject.GetComponentsInChildren<Renderer>().Where(x => !x.sharedMaterials.All(x => !x || x.shader.name == "Horizon Forge/Collider")).Select(x => x.transform));
        transforms.AddRange(this.gameObject.GetComponentsInChildren<Terrain>().Select(x => x.transform));

        // iterate hierarchy, group 
        foreach (var transform in transforms)
        {
            var t = transform;
            GameObject prefab = null;

            // move up hierarchy searching for prefab parent
            while (t != this.transform)
            {
                prefab = PrefabUtility.GetCorrespondingObjectFromOriginalSource(t.gameObject);
                if (prefab)
                    break;

                t = t.parent;
            }

            // default prefab to root gameobject instance
            if (!prefab)
            {
                prefab = PrefabUtility.GetCorrespondingObjectFromOriginalSource(this.gameObject);
                if (!prefab) prefab = this.gameObject;
            }

            // don't include duplicates
            if (children.Any(c => c.PrefabOrObject == prefab && c.InstanceRootTransform == t))
                continue;

            // add child
            children.Add(new ConvertToShrubChild()
            {
                PrefabOrObject = prefab,
                InstanceRootTransform = t
            });
        }

        return children;
    }

    public bool GetGeometry(out GameObject root)
    {
        root = null;
        if (!Validate())
            return false;

        // prefab
        var prefab = PrefabUtility.GetCorrespondingObjectFromOriginalSource(this.gameObject);
        if (prefab)
        {
            root = prefab;
            return true;
        }

        root = this.gameObject;
        return true;
    }

    public bool Validate()
    {
        var meshFilters = this.GetComponentsInChildren<MeshFilter>();
        if (meshFilters.Any(m => !m.gameObject.hideFlags.HasFlag(HideFlags.HideInHierarchy) && m.sharedMesh && m.sharedMesh.isReadable == false))
            return false;

        return true;
    }

    public string GetAssetHash()
    {
        return null;
    }
}
