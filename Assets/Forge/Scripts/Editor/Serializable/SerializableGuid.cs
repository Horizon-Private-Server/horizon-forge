using System;
using UnityEngine;

[Serializable]
public struct SerializableGuid
{
    [SerializeField]
    private string value;

    public Guid Guid
    {
        get => string.IsNullOrEmpty(value) ? Guid.Empty : new Guid(value);
        set => this.value = value.ToString();
    }

    public SerializableGuid(Guid guid)
    {
        value = guid.ToString();
    }

    public static SerializableGuid NewGuid()
        => new SerializableGuid(Guid.NewGuid());

    public override string ToString() => value;
}
