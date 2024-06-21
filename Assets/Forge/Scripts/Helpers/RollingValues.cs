using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class RollingVector3
{
    private Vector3[] _buffer;
    private int _index = 0;
    private int _count = 0;
    private Vector3 _value = Vector3.zero;

    public int Count => _count;
    public int MaxCount => _buffer.Length;
    public Vector3 Value => _value;

    public RollingVector3(int count)
    {
        _buffer = new Vector3[count];
    }

    public Vector3 Push(Vector3 value)
    {
        // store and increment index/count
        _buffer[_index] = value;
        if (_count < _buffer.Length) ++_count;
        _index = (_index + 1) % _buffer.Length;

        // recalculate value
        return _value = GetRollingValue();
    }

    private Vector3 GetRollingValue()
    {
        if (_count == 0) return Vector3.zero;

        var value = Vector3.zero;
        for (int i = 0; i < _count; ++i)
        {
            var idx = (_index - _count + i + _buffer.Length) % _buffer.Length;
            value += _buffer[idx];
        }

        return value / _count;
    }
}
