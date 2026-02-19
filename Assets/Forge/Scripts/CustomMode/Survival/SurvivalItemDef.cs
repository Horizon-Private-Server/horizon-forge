using System;
using System.Collections.Generic;
using System.Text;
using UnityEngine;

public class SurvivalItemDef : MonoBehaviour
{
    public bool IsCustomItem;
    public SurvivalDefaultItemOverrideEntry DefaultItemOverrides = new();
    public SurvivalItemEntry CustomItem = new();

    public string DefineName => IsCustomItem ? CustomItem.DefineName : DefaultItemOverrides.DefineName;
    public string GetDef() => IsCustomItem ? CustomItem.GetDef() : DefaultItemOverrides.GetDef();
    public string GetForwardDeclarations() => IsCustomItem ? CustomItem.GetForwardDeclarations() : DefaultItemOverrides.GetForwardDeclarations();
}
