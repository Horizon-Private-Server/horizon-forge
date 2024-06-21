using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;

public class WrenchAssetPostprocessor : AssetPostprocessor
{
    void OnPostprocessModel(GameObject g)
    {
        var labels = AssetDatabase.GetLabels(assetImporter);
        if (labels.Contains("wrench"))
        {
            // add lod group if applicable
            var renderers = g.GetComponentsInChildren<Renderer>();
            var highLodRenderers = renderers.Where(x => x.gameObject.name.StartsWith("high_lod")).ToArray();
            var lowLodRenderers = renderers.Where(x => x.gameObject.name.StartsWith("low_lod")).ToArray();
            var bangleRenderers = renderers.Where(x => x.gameObject.name.StartsWith("bangle_")).ToArray();

            foreach (var lowLod in lowLodRenderers)
                lowLod.gameObject.SetActive(false);

            foreach (var bangle in bangleRenderers)
                bangle.gameObject.SetActive(true);

            var isMoby = labels.Contains("Moby");
            var isTie = labels.Contains("Tie");
            var isShrub = labels.Contains("Shrub");
            var isTfrag = labels.Contains("Tfrag");

            if (isMoby) g.AddComponent<MobyAsset>();
            else if (isTie) g.AddComponent<TieAsset>();
            else if (isShrub) g.AddComponent<ShrubAsset>();
            else if (isTfrag) g.AddComponent<Tfrag>();
        }

    }
}
