using System.Linq;
using System.Text;
using UnityEngine;

[AddComponentMenu("Forge/Raids/Zone Definition")]
[DisallowMultipleComponent]
public class RaidsZoneDef : MonoBehaviour
{
    public Cuboid Cuboid;
    public DLRaidsDifficulties Difficulty;

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
        sb.AppendLine($"\t\t.Difficulty = {(int)this.Difficulty}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }
}
