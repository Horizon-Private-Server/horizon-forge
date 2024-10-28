using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;

[Serializable]
public class SerializableDictionary2<TKey, TValue> : Dictionary<TKey, TValue>, ISerializationCallbackReceiver
{
    [SerializeField]
    private List<TKey> keys = new List<TKey>();

    [SerializeField]
    private List<TValue> values = new List<TValue>();

    // save the dictionary to lists
    public void OnBeforeSerialize()
    {
        keys.Clear();
        values.Clear();
        foreach (KeyValuePair<TKey, TValue> pair in this)
        {
            keys.Add(pair.Key);
            values.Add(pair.Value);
        }
    }

    // load dictionary from lists
    public void OnAfterDeserialize()
    {
        this.Clear();

        if (keys.Count != values.Count)
            throw new System.Exception(string.Format("there are {0} keys and {1} values after deserialization. Make sure that both key and value types are serializable."));

        for (int i = 0; i < keys.Count; i++)
            this.Add(keys[i], values[i]);
    }
}

[System.Serializable]
public class SerializableDictionary<TKey, TValue> : Dictionary<TKey, TValue>, ISerializationCallbackReceiver
{
    [SerializeField] private List<SerializedDictionaryKVPProps<TKey, TValue>> dictionaryList = new();

    [SerializeField] public UnityEngine.Object Owner;

    void ISerializationCallbackReceiver.OnBeforeSerialize()
    {
        if (!Owner || UnityEditor.EditorUtility.IsDirty(Owner))
        {
            foreach (var kVP in this)
            {
                if (dictionaryList.FirstOrDefault(value => this.Comparer.Equals(value.Key, kVP.Key))
                    is SerializedDictionaryKVPProps<TKey, TValue> serializedKVP)
                {
                    serializedKVP.Value = kVP.Value;
                }
                else
                {
                    dictionaryList.Add(kVP);
                }
            }

            dictionaryList.RemoveAll(value => ContainsKey(value.Key) == false);

            for (int i = 0; i < dictionaryList.Count; i++)
            {
                dictionaryList[i].index = i;
            }
        }
    }

    void ISerializationCallbackReceiver.OnAfterDeserialize()
    {
        Clear();

        dictionaryList.RemoveAll(r => r.Key == null);

        foreach (var serializedKVP in dictionaryList)
        {
            if (!(serializedKVP.isKeyDuplicated = ContainsKey(serializedKVP.Key)))
            {
                Add(serializedKVP.Key, serializedKVP.Value);
            }
        }
    }

    public void SetPropertyKeyValue(SerializedProperty property, TKey key, TValue value)
    {
        var subprop = property.FindPropertyRelative("dictionaryList");
        var idx = dictionaryList.FindIndex(x => this.Comparer.Equals(x.Key, key));
        if (idx < 0)
        {
            idx = subprop.arraySize;
            subprop.arraySize = subprop.arraySize + 1;

            var elemprop = subprop.GetArrayElementAtIndex(idx);
            elemprop.FindPropertyRelative("Value").boxedValue = value;
            elemprop.FindPropertyRelative("Key").boxedValue = key;
            elemprop.FindPropertyRelative("index").boxedValue = idx;
            this[key] = value;
        }
        else
        {
            var elemprop = subprop.GetArrayElementAtIndex(idx).FindPropertyRelative("Value");
            if (!EqualityComparer<TValue>.Default.Equals(value, (TValue)elemprop.boxedValue))
            {
                elemprop.boxedValue = value;
                this[key] = value;
            }
        }
    }

    public new TValue this[TKey key]
    {
        get
        {
            if (ContainsKey(key))
            {
                return base[key];

                var duplicateKeysWithCount = dictionaryList.GroupBy(item => item.Key)
                                                           .Where(group => group.Count() > 1)
                                                           .Select(group => new { Key = group.Key, Count = group.Count() });

                foreach (var duplicatedKey in duplicateKeysWithCount)
                {
                    Debug.LogError($"Key '{duplicatedKey.Key}' is duplicated {duplicatedKey.Count} times in the dictionary.");
                }

                int idx = dictionaryList.FindIndex(x => Comparer.Equals(x.Key, key));
                if (idx < 0)
                {

                }
                return dictionaryList.First(x => Comparer.Equals(x.Key, key)).Value;
            }
            else
            {
                //Debug.LogError($"Key '{key}' not found in dictionary.");
                return default(TValue);
            }
        }
        set
        {
            base[key] = value;
        }
    }

    [System.Serializable]
    public class SerializedDictionaryKVPProps<TypeKey, TypeValue>
    {
        public TypeKey Key;
        public TypeValue Value;

        public int index;
        public bool isKeyDuplicated;

        public SerializedDictionaryKVPProps(TypeKey key, TypeValue value) { this.Key = key; this.Value = value; }

        public static implicit operator SerializedDictionaryKVPProps<TypeKey, TypeValue>(KeyValuePair<TypeKey, TypeValue> kvp)
            => new SerializedDictionaryKVPProps<TypeKey, TypeValue>(kvp.Key, kvp.Value);
        public static implicit operator KeyValuePair<TypeKey, TypeValue>(SerializedDictionaryKVPProps<TypeKey, TypeValue> kvp)
            => new KeyValuePair<TypeKey, TypeValue>(kvp.Key, kvp.Value);
    }
}

[Serializable]
public class SerializableStringDictionary : SerializableDictionary<string, string>
{

}
