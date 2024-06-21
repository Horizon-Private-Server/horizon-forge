using Newtonsoft.Json.Linq;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.Networking;

public static class ForgeUpdater
{
    static Queue<Action> _actionQueue = new Queue<Action>();

    [InitializeOnLoadMethod]
    static void Initialize()
    {
        EditorApplication.update -= EditorTick;
        EditorApplication.update += EditorTick;
    }

    /// <summary>
    /// ported from https://github.com/RatchetModding/Replanetizer/blob/d1c0f23eee9802117a89826081bd01ec5486e6aa/Replanetizer/Frames/UpdateInfoFrame.cs
    /// </summary>
    [MenuItem("Forge/Check for Update", priority = 10001)]
    public static async void CheckForUpdate()
    {
        try
        {
            using (var handler = new HttpClientHandler())
            {
                handler.UseDefaultCredentials = true;

                using (HttpClient client = new HttpClient(handler))
                {
                    //Github wants a user agent, otherwise it returns code 403 Forbidden
                    var requestMessage = new HttpRequestMessage(HttpMethod.Get, "https://api.github.com/repos/Horizon-Private-Server/horizon-forge-dl/releases/latest");
                    requestMessage.Headers.Add("User-Agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:47.0) Gecko/20100101 Firefox/47.0");

                    var response = await client.SendAsync(requestMessage);
                    response.EnsureSuccessStatusCode();
                    string content = await response.Content.ReadAsStringAsync();

                    JObject data = (JObject)Newtonsoft.Json.JsonConvert.DeserializeObject(content);
                    if (data != null)
                    {
                        string newestReleaseTag = (string?)data["tag_name"];
                        if (newestReleaseTag != null && newestReleaseTag != Constants.ForgeVersion)
                        {
                            if (EditorUtility.DisplayDialog("Forge Updater", $"There is a new update available. Would you like to update to {newestReleaseTag}?", "Update", "Cancel"))
                            {
                                Update(data);
                            }
                        }
                        else
                        {
                            Debug.Log($"No updates detected.");
                        }
                    }
                    else
                    {
                        Debug.Log($"Unable to parse github api response.");
                    }
                }
            }
        }
        catch (Exception e)
        {
            Debug.LogError("Failed to check for new version. Error: " + e.Message);
        }
    }

    private static async void Update(JObject data)
    {
        var forgeAssetsPath = FolderNames.ForgeFolder;
        var forgeBackupAssetsPath = "assets_forge_update_backup";

        // can't update if we have a git repo
        // use git pull and manual install of tools
        if (HasGitRepo())
        {
            Debug.LogError("Updating with git repo detected. Please use git pull and manually install the latest tools package from the releases tab.");
            return;
        }

        // update
        using (var handler = new HttpClientHandler())
        {
            handler.UseDefaultCredentials = true;

            using (HttpClient client = new HttpClient(handler))
            {
                // query github api for releases
                if (data == null)
                {
                    HttpRequestMessage requestMessage = new HttpRequestMessage(HttpMethod.Get, "https://api.github.com/repos/Horizon-Private-Server/horizon-forge-dl/releases/latest");

                    //Github wants a user agent, otherwise it returns code 403 Forbidden
                    requestMessage.Headers.Add("User-Agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:47.0) Gecko/20100101 Firefox/47.0");

                    HttpResponseMessage response = await client.SendAsync(requestMessage);
                    response.EnsureSuccessStatusCode();
                    string content = await response.Content.ReadAsStringAsync();

                    data = (JObject)Newtonsoft.Json.JsonConvert.DeserializeObject(content);
                }

                if (data != null)
                {
                    JArray assets = (JArray)data["assets"];
                    string toolsZipUrl = null;
                    string forgeZipUrl = null;

                    // find tools asset
                    // todo: add support for linux tools package
                    if (assets != null)
                    {
                        foreach (JObject asset in assets)
                        {
                            if ((string)asset["name"] == "tools-windows.zip")
                            {
                                toolsZipUrl = (string)asset["browser_download_url"];
                            }
                            else if ((string)asset["name"] == "forge.zip")
                            {
                                forgeZipUrl = (string)asset["browser_download_url"];
                            }
                        }
                    }

                    // validate update
                    if (string.IsNullOrEmpty(forgeZipUrl))
                    {
                        Debug.LogError("Unable to find update forge zip url");
                        return;
                    }

                    // tools update is optional -- though should be included in every release regardless
                    if (string.IsNullOrEmpty(toolsZipUrl))
                    {
                        Debug.LogWarning("Unable to find update tools package url");
                    }

                    try
                    {
                        AssetDatabase.StartAssetEditing();

                        // build paths
                        var tempPath = FolderNames.GetTempFolder();
                        var tempOutputPath = Path.Combine(tempPath, "testupdate");
                        var forgeZipPath = Path.Combine(tempPath, "forge.zip");
                        var toolsZipPath = Path.Combine(tempPath, "tools.zip");

                        // move current Assets/Forge/ folder into backup
                        // if update fails we can restore it
                        // otherwise we'll remove it at the end
                        if (Directory.Exists(forgeBackupAssetsPath)) Directory.Delete(forgeBackupAssetsPath, true);
                        if (Directory.Exists(forgeAssetsPath))
                            Directory.Move(forgeAssetsPath, forgeBackupAssetsPath);

                        // download and unzip forge update into temp folder
                        var zipBytes = await DownloadAsync(client, forgeZipUrl, "Downloading Forge Update...");
                        await EnqueueActionAsync(() => EditorUtility.DisplayProgressBar("Extracting Forge Update", "", 0.5f));
                        File.WriteAllBytes(forgeZipPath, zipBytes);
                        if (Directory.Exists(tempOutputPath)) Directory.Delete(tempOutputPath, true);
                        System.IO.Compression.ZipFile.ExtractToDirectory(forgeZipPath, tempOutputPath, true);

                        // remove existing Assets/Forge/ directory
                        // and copy update over
                        // update has a root folder we don't want, with new name each update
                        // grab first subdirectory and copy that
                        var forgeUpdateSubDir = Directory.GetDirectories(tempOutputPath).FirstOrDefault();
                        if (string.IsNullOrEmpty(forgeUpdateSubDir))
                            throw new InvalidOperationException("Update is empty");
                        IOHelper.CopyDirectory(forgeUpdateSubDir, Environment.CurrentDirectory);

                        // download and unzip tools package
                        if (!string.IsNullOrEmpty(toolsZipUrl))
                        {
                            var toolsBytes = await DownloadAsync(client, toolsZipUrl, "Downloading Tools Package...");
                            await EnqueueActionAsync(() => EditorUtility.DisplayProgressBar("Extracting Tools Package", "", 0.5f));
                            File.WriteAllBytes(toolsZipPath, toolsBytes);
                            System.IO.Compression.ZipFile.ExtractToDirectory(toolsZipPath, Environment.CurrentDirectory, true);
                        }

                        // success, no errors so remove backup
                        if (Directory.Exists(forgeBackupAssetsPath)) Directory.Delete(forgeBackupAssetsPath, true);
                        await EnqueueActionAsync(() => EditorUtility.DisplayDialog("Forge Updater", "Update Complete", "Close"));
                    }
                    catch (Exception ex)
                    {
                        // restore backup
                        if (Directory.Exists(forgeBackupAssetsPath))
                        {
                            if (Directory.Exists(forgeAssetsPath)) Directory.Delete(forgeAssetsPath, true);
                            Directory.Move(forgeBackupAssetsPath, forgeAssetsPath);
                        }

                        Debug.LogException(ex);
                    }
                    finally
                    {
                        await EnqueueActionAsync(() => EditorUtility.ClearProgressBar());
                        AssetDatabase.StopAssetEditing();
                        AssetDatabase.Refresh();
                    }
                }
                else
                {
                    Debug.Log($"Unable to parse github api response.");
                }
            }
        }
    }

    private static async Task<byte[]> DownloadAsync(HttpClient client, string url, string progressBarMessage = null)
    {
        var totalBytesRead = 0L;
        var readCount = 0L;
        var buffer = new byte[1024 * 64];
        var isMoreToRead = true;
        var progressUnit = 1024f * 1024f;
        var progressUnitStr = "mb";
        var showProgressBar = !string.IsNullOrEmpty(progressBarMessage);

        //Github wants a user agent, otherwise it returns code 403 Forbidden
        var requestMessage = new HttpRequestMessage(HttpMethod.Get, url);
        requestMessage.Headers.Add("User-Agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:47.0) Gecko/20100101 Firefox/47.0");

        var response = await client.SendAsync(requestMessage);
        response.EnsureSuccessStatusCode();

        var contentLength = response.Content.Headers.ContentLength;
        if (!contentLength.HasValue) return null;

        var destination = new byte[contentLength.Value];
        using (var destStream = new MemoryStream(destination, true))
        {
            using (var download = await response.Content.ReadAsStreamAsync())
            {
                do
                {
                    var bytesRead = await download.ReadAsync(buffer, 0, buffer.Length);
                    if (bytesRead == 0)
                    {
                        isMoreToRead = false;
                        continue;
                    }

                    await destStream.WriteAsync(buffer, 0, bytesRead);
                    totalBytesRead += bytesRead;

                    // update progress bar
                    if (showProgressBar && (readCount % 50) == 0)
                    {
                        var relativeProgress = (float)totalBytesRead / contentLength.Value;
                        await EnqueueActionAsync(() =>
                        {
                            EditorUtility.DisplayProgressBar(progressBarMessage, $"{totalBytesRead / progressUnit:f2}{progressUnitStr} / {contentLength.Value / progressUnit:f2}{progressUnitStr}", relativeProgress);
                        });
                    }

                    readCount += 1;
                }
                while (isMoreToRead);

                // clear progress bar
                if (showProgressBar)
                {
                    await EnqueueActionAsync(() =>
                    {
                        EditorUtility.ClearProgressBar();
                    });
                }
            }
        }

        return destination;
    }

    private static async Task EnqueueActionAsync(Action action)
    {
        lock (_actionQueue)
        {
            _actionQueue.Enqueue(action);
        }

        // yield let editor updatetick execute
        await Task.Delay(1);
    }

    private static bool HasGitRepo()
    {
        return Directory.Exists(Path.Combine(Environment.CurrentDirectory, ".git"));
    }

    private static void EditorTick()
    {
        if (_actionQueue.Count == 0) return;

        lock (_actionQueue)
        {
            while (_actionQueue.TryDequeue(out var action))
                action?.Invoke();
        }
    }
}
