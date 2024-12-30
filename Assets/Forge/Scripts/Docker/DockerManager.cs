using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;
using DotNet.Testcontainers.Images;


#if DOCKER

using DotNet.Testcontainers.Builders;
using DotNet.Testcontainers.Containers;

[ExecuteInEditMode]
public class DockerManager : MonoBehaviour
{
    public static DockerManager Singleton { get; private set; }
    public static readonly string[] SleepInfinity = { "/bin/sh", "-c", "trap : TERM INT; sleep infinity & wait" };

    private IContainer container;

    public static DockerManager GetOrCreate()
    {
        if (Singleton) return Singleton;

        Singleton = FindObjectOfType<DockerManager>();
        if (Singleton) return Singleton;

        var go = new GameObject("Docker Manager");
        go.hideFlags = HideFlags.HideAndDontSave;
        return Singleton = go.AddComponent<DockerManager>();
    }

    private void Update()
    {
        // destroy duplicate
        if (Singleton && Singleton != this)
        {
            DestroyImmediate(this.gameObject);
            return;
        }

        Singleton = this;
    }

    private void OnDestroy()
    {
        if (container != null)
        {
            var c = container;
            _ = c.StopAsync().ContinueWith((_) => c.DisposeAsync());
            container = null;
        }
    }

    public async void Validate()
    {
        if (container == null) return;

        try
        {
            await container.GetExitCodeAsync();
        }
        catch
        {
            // bad container
            container = null;
        }
    }

    public TestcontainersStates? GetStatus()
    {
        return container?.State;
    }

    public bool ContainerReady() => container != null && container.State == TestcontainersStates.Running;
    public bool ContainerStarting() => container != null && (container.State == TestcontainersStates.Undefined || container.State == TestcontainersStates.Restarting);

    public async Task<ExecResult> ExecuteAsync(params string[] commands)
    {
        if (container == null) return default;
        return await container.ExecAsync(commands)
          .ConfigureAwait(true);
    }

    public async Task Run()
    {
        if (container != null)
        {
            //await container.StopAsync();
            await container.DisposeAsync();
            container = null;
        }

        ContainerBuilder builder = new ContainerBuilder()
              // use particular version always 
              .WithImage("dnawrkshp/ps2dev-libdl:latest")
              .WithImagePullPolicy(PullPolicy.Always)
              // name it nicely
              .WithName("FORGE_PS2DEV")
              .WithBindMount(Path.GetFullPath(Path.Combine(Environment.CurrentDirectory, FolderNames.BinaryFolder)), "/levels")
              // with some configuration
              //.WithEnvironment("DOCKER_HOST", "unix:///var/run/docker.sock")
              //.WithWaitStrategy(Wait.ForUnixContainer())
              //.WithReuse(true)
              .WithCleanUp(true)
              .WithReuse(true)
              .WithEntrypoint(SleepInfinity)
              .WithOutputConsumer(Consume.RedirectStdoutAndStderrToConsole());

        //builder = builder.WithPortBinding(5432, 5432);
        container = builder.Build();
        await container.StartAsync().ConfigureAwait(true);
    }

}

#endif
