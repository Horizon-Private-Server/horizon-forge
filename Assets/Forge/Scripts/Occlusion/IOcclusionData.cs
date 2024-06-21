using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

public interface IOcclusionData
{
    public static List<IOcclusionData> AllOcclusionDatas = new List<IOcclusionData>();

    [InitializeOnLoadMethod]
    private static void OnInitialized()
    {
        AllOcclusionDatas = GameObject.FindObjectsOfType<Transform>().Select(x => x.GetComponent<IOcclusionData>()).Where(x => x != null).ToList();
    }

    public Vector3[] Octants { get; set; }
    public int OcclusionId { get; set; }
    public OcclusionDataType OcclusionType { get; }
    public int UniqueOcclusionId => (OcclusionId + 1) + ((int)OcclusionType * 256 * 256);

    public void OnPostBake();
    public void OnPreBake(Color32 uidColor);

    public static void ForceUniqueOcclusionIds()
    {
        // pass pre event to OcclusionData
        var occlusionDatas = AllOcclusionDatas;
        var occlusionDatasWithDupeIds = occlusionDatas.Where(x => AllOcclusionDatas.Count(y => y.OcclusionType == x.OcclusionType && y.OcclusionId == x.OcclusionId) > 1).ToList();
        if (occlusionDatasWithDupeIds.Count > 0)
        {
            HashSet<int> existingIds = AllOcclusionDatas.Select(x => x.UniqueOcclusionId).ToHashSet();

            foreach (var occlDataWithDupe in occlusionDatasWithDupeIds)
            {
                while (existingIds.Contains(occlDataWithDupe.UniqueOcclusionId))
                    occlDataWithDupe.OcclusionId += 1;

                existingIds.Add(occlDataWithDupe.UniqueOcclusionId);
            }
        }
    }

    public static void ForceUniqueOcclusionId(IOcclusionData occlusionData)
    {
        // pass pre event to OcclusionData
        if (AllOcclusionDatas.Any(x => x.OcclusionType == occlusionData.OcclusionType && x.OcclusionId == occlusionData.OcclusionId && x != occlusionData))
        {
            HashSet<int> existingIds = AllOcclusionDatas.Where(x => x.OcclusionType == occlusionData.OcclusionType).Select(x => x.OcclusionId).ToHashSet();
            int occlId = occlusionData.OcclusionId;
            while (existingIds.Contains(occlId))
                occlId += 1;

            existingIds.Add(occlId);
            occlusionData.OcclusionId = occlId;
        }
    }
}

public enum OcclusionDataType
{
    Tie,
    Tfrag,
    Moby
}
