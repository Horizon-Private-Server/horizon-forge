using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class ConvertToShrub : MonoBehaviour
{
    public int GroupId;
    public float RenderDistance = 64;
    [ColorUsage(false)] public Color Tint = Color.white * 0.5f;
    [Tooltip("If true and Export Shrubs is enabled, the DZO exporter will automatically include this object.")] public bool DZOExportWithShrubs = true;

    public bool GetGeometry(out GameObject root)
    {
        root = null;
        if (!Validate())
            return false;

        // prefab
        var prefab = PrefabUtility.GetCorrespondingObjectFromSource(this.gameObject);
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
