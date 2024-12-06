using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public class BuildState
{
    public string MapSceneName { get; }
    public int RacVersion { get; }
    public GameRegion Region { get; }
    public List<int> MobyOClasses { get; set; } = new List<int>();

    public BuildState(string mapSceneName, int racVersion, GameRegion region)
    {
        MapSceneName = mapSceneName;
        RacVersion = racVersion;
        Region = region;
    }
}

public interface IBuildHook
{
    bool IsEnabled { get; }
    void Configure(BuildState state);


    public static void Run(BuildState state)
    {
        var hooks = GameObject.FindObjectsOfType<MonoBehaviour>().Select(x => x.GetComponent<IBuildHook>()).Where(x => x != null).ToArray();
        foreach (var hook in hooks)
        {
            if (!hook.IsEnabled) continue;
            hook.Configure(state);
        }
    }
}
