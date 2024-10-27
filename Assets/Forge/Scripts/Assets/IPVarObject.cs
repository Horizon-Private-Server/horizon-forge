using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public interface IPVarObject
{
    int GetRCVersion();
    byte[] GetPVarData();
    Cuboid[] GetPVarCuboidRefs();
    Moby[] GetPVarMobyRefs();
    Spline[] GetPVarSplineRefs();
    Area[] GetPVarAreaRefs();
    PathGraph[] GetPVarPathGraphRefs();
    string[] GetPVarStrings();
    PvarOverlay GetPVarOverlay();

    void SetPVarData(byte[] pvarData);
    void SetPVarCuboidRefs(Cuboid[] cuboidRefs);
    void SetPVarMobyRefs(Moby[] mobyRefs);
    void SetPVarSplineRefs(Spline[] splineRefs);
    void SetPVarAreaRefs(Area[] areaRefs);
    void SetPVarPathGraphRefs(PathGraph[] pathGraphRefs);
    void SetPVarStrings(string[] strings);
}
