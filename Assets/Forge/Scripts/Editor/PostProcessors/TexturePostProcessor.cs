using UnityEditor;
using UnityEngine;
using System.Collections;
using UnityEngine.Experimental.Rendering;
using System.Collections.Generic;

public class TexturePostProcessor : AssetPostprocessor
{
    private static readonly Dictionary<TextureImporterFormat, TextureImporterFormat> _formatRemap = new()
    {
        { TextureImporterFormat.RGB24, TextureImporterFormat.RGBA32 },
        { TextureImporterFormat.RGB16, TextureImporterFormat.RGBA32 },
        { TextureImporterFormat.RGB48, TextureImporterFormat.RGBA64 },
    };

    private void OnPreprocessTexture()
    {
        TextureImporter textureImporter = (TextureImporter)assetImporter;
        TextureImporterPlatformSettings settings = textureImporter.GetDefaultPlatformTextureSettings();

        // force alpha channel
        // otherwise glb exporter will encode as jpg
        // causing massive drop in quality
        if (_formatRemap.TryGetValue(settings.format, out var remapFormat)) 
        {
            settings.format = remapFormat;
            textureImporter.SetPlatformTextureSettings(settings);
        }
    }
}
