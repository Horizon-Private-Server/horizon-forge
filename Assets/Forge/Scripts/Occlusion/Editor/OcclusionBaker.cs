using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;

public static class OcclusionBaker
{
    [MenuItem("Forge/Tools/Occlusion/Clear Occlusion For Selected")]
    public static void ClearOcclusion()
    {
        var selectedOcclusionDatas = UnityHelper.GetAllOcclusionDataInSelection();
        if (selectedOcclusionDatas == null || selectedOcclusionDatas.Count == 0)
        {
            EditorUtility.DisplayDialog("Occlusion Builder", "Please select at least one object with OcclusionData!", "Ok");
            return;
        }

        foreach (var selectedOcclusionData in selectedOcclusionDatas)
            selectedOcclusionData.Octants = new Vector3[0];
    }

    [MenuItem("Forge/Tools/Occlusion/Bake Occlusion For Selected")]
    public static void BakeOcclusion()
    {
        ComputeBuffer parsedIdsBuffer = null;

        var selectedOcclusionDatas = UnityHelper.GetAllOcclusionDataInSelection();
        if (selectedOcclusionDatas == null || selectedOcclusionDatas.Count == 0)
        {
            EditorUtility.DisplayDialog("Occlusion Builder", "Please select at least one object with OcclusionData!", "Ok");
            return;
        }

        var bakeSettings = GameObject.FindObjectOfType<OcclusionBakeSettings>();
        if (!bakeSettings)
            bakeSettings = new OcclusionBakeSettings();

        var renderResolution = (int)Mathf.Pow(2, (int)bakeSettings.Resolution + 5);
        //var clipPixelCount = (int)Math.Max(bakeSettings.ClipPixelCount, bakeSettings.ClipPercent * renderResolution * renderResolution);
        var octants = UnityHelper.GetAllOctants();
        var graph = GameObject.FindObjectOfType<OcclusionGraph>();

        var shader = Shader.Find("Shader Graphs/OcclusionBakeRender");

        var lastOctantsPerMs = 0.0;
        var lastOctantsPerMsIdx = 0;

        // render texture
        var rtColor = new RenderTexture(renderResolution, renderResolution, 0, RenderTextureFormat.ARGB64, RenderTextureReadWrite.Default);
        rtColor.filterMode = FilterMode.Point;
        rtColor.antiAliasing = 1;
        rtColor.anisoLevel = 16;
        rtColor.autoGenerateMips = false;
        //rtColor.memorylessMode = RenderTextureMemoryless.Color;

        // camera
        var occlusionCameraGo = new GameObject("occlusion camera");
        var occlusionCamera = occlusionCameraGo.AddComponent<Camera>();

        occlusionCamera.fieldOfView = bakeSettings.RenderFov;
        occlusionCamera.backgroundColor = Color.black;
        occlusionCamera.allowMSAA = false;
        occlusionCamera.clearFlags = CameraClearFlags.Color;
        occlusionCamera.cullingMask = bakeSettings.CullingMask;
        occlusionCamera.farClipPlane = bakeSettings.RenderDistance;

        //
        Func<Vector3, string> getOctantKey = (Vector3 octant) =>
        {
            var x = Mathf.RoundToInt(octant.x / 4);
            var y = Mathf.RoundToInt(octant.y / 4);
            var z = Mathf.RoundToInt(octant.z / 4);

            return $"{x}.{y}.{z}";
        };

        Shader.EnableKeyword("_OCCLUSION");
        try
        {
            // pass pre event to OcclusionData
            var allOcclusionDatas = IOcclusionData.AllOcclusionDatas;
            var occlusionDatas = selectedOcclusionDatas; // GameObject.FindObjectsOfType<OcclusionData>();
            var occlusionDatasWithDupeIds = occlusionDatas.Where(x => allOcclusionDatas.Count(y => y.OcclusionType == x.OcclusionType && y.OcclusionId == x.OcclusionId) > 1).ToList();
            if (occlusionDatasWithDupeIds.Count > 0)
            {
                if (EditorUtility.DisplayDialog("Occlusion Builder", "Found duplicate occlusion ids. Would you like to make them unique?", "Yes", "No"))
                {
                    HashSet<int> existingIds = allOcclusionDatas.Select(x => x.UniqueOcclusionId).ToHashSet();
                    foreach (var occlDataWithDupe in occlusionDatasWithDupeIds)
                    {
                        while (existingIds.Contains(occlDataWithDupe.UniqueOcclusionId))
                            occlDataWithDupe.OcclusionId += 1;

                        existingIds.Add(occlDataWithDupe.UniqueOcclusionId);
                    }
                }
            }

            var occlusionDataIds = occlusionDatas.Select(x => x.UniqueOcclusionId).ToArray();
            var newOcclusionOctantsById = occlusionDataIds.ToDictionary(x => x, x => new Dictionary<Vector3, bool>(octants.Count * 3 * 3));
            var maxOcclusionId = 256 * 256 * 3;
            var existingOctants = octants.ToDictionary(x => x, x => false);
            var colorInc = new Color32();
            var allOctantsInNeighborhood = new Vector3?[(int)Mathf.Pow((bakeSettings.FeatherOctantRadius * 2 + 1), 3)];
            Vector3[] renderPoints = new Vector3[8];

            // 
            var parsedIdsCount = Mathf.CeilToInt(maxOcclusionId / 32f) * 32;
            parsedIdsBuffer = new ComputeBuffer(parsedIdsCount, sizeof(int), ComputeBufferType.Default);
            var sampleParsedIds = new int[parsedIdsCount];
            //var parsedIds = new int[parsedIdsCount];

            // compute
            var computeShader = AssetDatabase.LoadAssetAtPath<ComputeShader>(Path.Combine(FolderNames.ForgeFolder, "Occlusion", "OcclusionParse.compute"));
            computeShader.SetInt("Width", rtColor.width);
            computeShader.SetInt("Height", rtColor.height);
            computeShader.SetTexture(0, "In", rtColor);
            computeShader.SetBuffer(0, "OutIds", parsedIdsBuffer);
            computeShader.SetBuffer(1, "OutIds", parsedIdsBuffer);

            foreach (var occlusionData in allOcclusionDatas)
            {
                // encode occlusion id as color
                // occlusion id of 0 is possible so add 1 so that a color of 0,0,0 (black) is reserved for unused occluders.
                byte g = (byte)((occlusionData.OcclusionId+1) / 256f);
                byte r = (byte)((occlusionData.OcclusionId+1) - (g * 256));
                colorInc = new Color32(r, g, (byte)occlusionData.OcclusionType, 255);

                occlusionData.OnPreBake(colorInc);
            }

            int oi = 0;
            var sw = Stopwatch.StartNew();
            var cache = new Dictionary<string, List<int>>();
            foreach (var octant in octants)
            {
                // get neighbors
                var center = octant + Vector3.one * 2f + bakeSettings.OctantOffset;
                GetNeighbors(octant, allOctantsInNeighborhood, bakeSettings.FeatherOctantRadius);
                for (int i = 0; i < allOctantsInNeighborhood.Length; ++i)
                    if (!existingOctants.ContainsKey(allOctantsInNeighborhood[i].Value))
                        allOctantsInNeighborhood[i] = null;

                // all four corners
                renderPoints[0] = center + (Vector3.up * 2 + Vector3.forward * 2 + Vector3.right * 2);
                renderPoints[1] = center + (Vector3.up * 2 + Vector3.forward * 2 + -Vector3.right * 2);
                renderPoints[2] = center + (Vector3.up * 2 + -Vector3.forward * 2 + Vector3.right * 2);
                renderPoints[3] = center + (Vector3.up * 2 + -Vector3.forward * 2 + -Vector3.right * 2);
                renderPoints[4] = center + (-Vector3.up * 2 + Vector3.forward * 2 + Vector3.right * 2);
                renderPoints[5] = center + (-Vector3.up * 2 + Vector3.forward * 2 + -Vector3.right * 2);
                renderPoints[6] = center + (-Vector3.up * 2 + -Vector3.forward * 2 + Vector3.right * 2);
                renderPoints[7] = center + (-Vector3.up * 2 + -Vector3.forward * 2 + -Vector3.right * 2);

                for (int i = 0; i < renderPoints.Length; ++i)
                {
                    var p = renderPoints[i];
                    var k = getOctantKey(p);
                    var b = 0;

                    if (!cache.ContainsKey(k) && (!graph || graph.CanSeeAnyNode(p)))
                    {
                        // clear parsedIdsBuffer
                        computeShader.Dispatch(1, Mathf.CeilToInt(parsedIdsCount / 32f), 1, 1);

                        // render forward
                        Render(occlusionCamera, rtColor, p, Quaternion.AngleAxis(0, Vector3.up), b);
                        ParseOcclusionIds(computeShader, rtColor.width, rtColor.height);

                        // render right
                        Render(occlusionCamera, rtColor, p, Quaternion.AngleAxis(90f, Vector3.up), b);
                        ParseOcclusionIds(computeShader, rtColor.width, rtColor.height);

                        // render backward
                        Render(occlusionCamera, rtColor, p, Quaternion.AngleAxis(180f, Vector3.up), b);
                        ParseOcclusionIds(computeShader, rtColor.width, rtColor.height);

                        // render left
                        Render(occlusionCamera, rtColor, p, Quaternion.AngleAxis(-90f, Vector3.up), b);
                        ParseOcclusionIds(computeShader, rtColor.width, rtColor.height);

                        // render up
                        Render(occlusionCamera, rtColor, p, Quaternion.AngleAxis(-90f, Vector3.right), b);
                        ParseOcclusionIds(computeShader, rtColor.width, rtColor.height);

                        // render bottom
                        Render(occlusionCamera, rtColor, p, Quaternion.AngleAxis(90f, Vector3.right), b);
                        ParseOcclusionIds(computeShader, rtColor.width, rtColor.height);

                        // store visible ids
                        parsedIdsBuffer.GetData(sampleParsedIds);
                        var parsedIds = new List<int>();
                        foreach (var occlusionDataId in occlusionDataIds)
                            if (sampleParsedIds[occlusionDataId] > 0)
                                parsedIds.Add(occlusionDataId);
                        cache.Add(k, parsedIds);
                    }
                }

                foreach (var p in renderPoints)
                {
                    var k = getOctantKey(p);
                    if (cache.TryGetValue(k, out var parsedIds))
                    {
                        foreach (var parsedId in parsedIds)
                        {
                            if (newOcclusionOctantsById.TryGetValue(parsedId, out var instanceOctants))
                            {
                                // add all neighbors
                                foreach (var o in allOctantsInNeighborhood)
                                    if (o.HasValue && !instanceOctants.ContainsKey(o.Value))
                                        instanceOctants.Add(o.Value, true);
                            }
                        }
                    }
                }

                ++oi;
                if (sw.Elapsed.TotalMilliseconds > 500)
                {
                    lastOctantsPerMs = sw.Elapsed.TotalMilliseconds / (oi - lastOctantsPerMsIdx);
                    lastOctantsPerMsIdx = oi;
                    sw.Restart();
                }

                if (EditorUtility.DisplayCancelableProgressBar("Computing Occlusion", $"{oi} / {octants.Count}... [{lastOctantsPerMs:.02} ms/octant]", (float)oi / octants.Count))
                {
                    break;
                }
            }

            foreach (var occlusionData in occlusionDatas)
            {
                if (newOcclusionOctantsById.TryGetValue(occlusionData.UniqueOcclusionId, out var newOctants))
                    occlusionData.Octants = newOctants.Select(x => x.Key).ToArray();
                else
                    occlusionData.Octants = new Vector3[0];
            }

            foreach (var occlusionData in allOcclusionDatas)
            {
                occlusionData.OnPostBake();
            }
        }
        catch (Exception ex)
        {
            UnityEngine.Debug.LogError(ex);
        }
        finally
        {
            Shader.DisableKeyword("_OCCLUSION");
            rtColor.Release();
            parsedIdsBuffer?.Release();
            GameObject.DestroyImmediate(occlusionCameraGo);
            EditorUtility.ClearProgressBar();
        }
    }

    private static void GetNeighbors(Vector3 octant, Vector3?[] octants, int radius)
    {
        int max = radius * 2 + 1;
        for (int x = 0; x < max; ++x)
        {
            for (int y = 0; y < max; ++y)
            {
                for (int z = 0; z < max; ++z)
                {
                    octants[x + (y * max) + (z * max * max)] = octant + (new Vector3(x - radius, y - radius, z - radius) * 4f);
                }
            }
        }
    }

    private static void ParseOcclusionIds(ComputeShader compute, int width, int height)
    {
        // run
        compute.Dispatch(0, Mathf.CeilToInt(width / 32f), Mathf.CeilToInt(height / 32f), 1);
    }

    private static void SaveTexture(RenderTexture rt, string path)
    {
        Texture2D tex = new Texture2D(rt.width, rt.height, TextureFormat.ARGB32, false);
        RenderTexture.active = rt;
        tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        tex.Apply();

        byte[] bytes = tex.EncodeToPNG();
        System.IO.File.WriteAllBytes(path, bytes);
        AssetDatabase.ImportAsset(path);
    }

    private static void Render(Camera camera, RenderTexture rtColor, Vector3 position, Quaternion rotation, float backwardsAmount)
    {
        camera.transform.position = position - (rotation * Vector3.forward * backwardsAmount);
        camera.transform.rotation = rotation;
        camera.targetTexture = rtColor;
        camera.depthTextureMode = DepthTextureMode.None;
        camera.Render();

        //SaveTexture(rtColor, $"Assets/Forge/Occlusion/Renders/render-{ri++}.png");
    }
}
