using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public interface ICodeGen
{
    bool IsEnabled { get; }
    int CodeGenOrder { get; }

    void Configure(string buildFolder, CodeGenState state);
}
