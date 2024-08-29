using DotNet.Testcontainers.Builders;
using DotNet.Testcontainers.Containers;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public class DockerManager : MonoBehaviour
{
    public static DockerManager Singleton { get; private set; }
    public static readonly string[] SleepInfinity = { "/bin/sh", "-c", "trap : TERM INT; sleep infinity & wait" };

    private IContainer container;

    private void Awake()
    {
        // destroy duplicate
        if (Singleton && Singleton != this)
        {
            DestroyImmediate(this.gameObject);
            return;
        }

        Singleton = this;
    }

    public TestcontainersStates? GetStatus()
    {
        return container?.State;
    }

    public bool ContainerReady() => container != null && container.State == TestcontainersStates.Running;

    public async Task<ExecResult> ExecuteAsync(params string[] commands)
    {
        if (container == null) return default;
        return await container.ExecAsync(commands)
          .ConfigureAwait(true);
    }

    public void Run()
    {
        if (container != null)
        {
            _ = container.StopAsync();
            container = null;
        }

        ContainerBuilder builder = new ContainerBuilder()
              // use particular version always 
              .WithImage("ps2dev/ps2dev:v1.2.0")
              // name it nicely
              .WithName("FORGE_PS2DEV")
              .WithBindMount(Path.GetFullPath(Path.Combine(Environment.CurrentDirectory, FolderNames.BinaryFolder)), "/levels")
              // with some configuration
              //.WithEnvironment("DOCKER_HOST", "unix:///var/run/docker.sock")
              //.WithWaitStrategy(Wait.ForUnixContainer())
              //.WithReuse(true)
              .WithEntrypoint(SleepInfinity)
              .WithOutputConsumer(Consume.RedirectStdoutAndStderrToConsole());

        //builder = builder.WithPortBinding(5432, 5432);
        container = builder.Build();
        _ = container.StartAsync().ConfigureAwait(true);
    }

}
