using System.Linq;
using System.Text;
using UnityEngine;

[AddComponentMenu("Forge/Raids/Raids Zone Definition")]
[DisallowMultipleComponent]
public class RaidsZoneDef : MonoBehaviour
{
    public Cuboid Cuboid;
    public DLRaidsDifficulties Difficulty;
    [Tooltip("Show the zone name and difficulty on the local HUD for 3 seconds when entering this zone.")]
    public bool ShowEntryOverlay;

    public string Name => gameObject.name;

    public string GetDef()
    {
        var sb = new StringBuilder();

        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var cuboidIdx = mapConfig.GetIndexOfCuboid(this.Cuboid);
        if (cuboidIdx < 0)
            Debug.LogWarning($"Missing cuboid for Zone {this.Name}");

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.CuboidIdx = {cuboidIdx},");
        sb.AppendLine($"\t\t.Difficulty = {(int)this.Difficulty},");
        sb.AppendLine($"\t\t.Name = \"{Name.MaxLength(31).Escape()}\",");
        sb.AppendLine($"\t\t.ShowEntryOverlay = {(ShowEntryOverlay ? 1 : 0)}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }
}
