using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class ColliderPostProcessor : AssetPostprocessor
{
    private void OnPostprocessModel(GameObject gameObject)
    {
        var labels = AssetDatabase.GetLabels(assetImporter);
        if (labels.Contains("collider") || labels.Contains("Collider"))
        {
            var shader = Shader.Find("Horizon Forge/Collider");
            var renderers = gameObject.GetComponentsInChildren<Renderer>();
            foreach (var renderer in renderers)
            {
                foreach (var material in renderer.sharedMaterials)
                {
                    if (material.shader == shader) continue;

                    var id = 0;
                    if (int.TryParse(material.name.Split(new char[] { '_', '.' }).ElementAtOrDefault(1), System.Globalization.NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var hId))
                        id = hId;

                    material.shader = shader;
                    material.SetInteger("_ColId", id);
                }
            }
        }
    }
}
