using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public interface IPVarObject
{
    int GetRCVersion();
    byte[] GetPVarData();
    SerializableStringDictionary GetPVarValues();
    SerializableMonoBehaviourDictionary GetPVarReferences();
    string[] GetPVarStrings();
    PvarOverlay GetPVarOverlay();

    void SetPVarData(byte[] pvarData);
    void SetPVarValues(SerializableStringDictionary pvarValues);
    void SetPVarReferences(SerializableMonoBehaviourDictionary pvarRefs);
    void SetPVarStrings(string[] strings);
}
