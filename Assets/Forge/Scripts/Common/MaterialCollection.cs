using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public class MaterialCollection
{
    List<Material> _materials = new List<Material>();
    Dictionary<Hash128, Material> _materialHashes = new Dictionary<Hash128, Material>();
    Func<Material, Hash128> _hashFunc;

    public int Count => _materials.Count;
    public Material this[int i] => _materials[i];

    public MaterialCollection(Func<Material, Hash128> hashFunc)
    {
        _hashFunc = hashFunc;
    }

    public int GetOrInsert(Material material)
    {
        var hash = _hashFunc(material);
        if (_materialHashes.TryGetValue(hash, out var mat))
            return _materials.IndexOf(mat);

        _materials.Add(material);
        _materialHashes[hash] = material;
        return _materials.Count - 1;
    }

}
