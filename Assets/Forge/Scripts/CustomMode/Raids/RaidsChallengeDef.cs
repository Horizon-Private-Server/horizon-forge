using UnityEngine;

[AddComponentMenu("Forge/Raids/Challenge Definition")]
[DisallowMultipleComponent]
public class RaidsChallengeDef : MonoBehaviour
{
    public string Description;

    public string Name => gameObject.name;
}
