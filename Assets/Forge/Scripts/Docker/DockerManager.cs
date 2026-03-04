using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

#if DOCKER

using DotNet.Testcontainers.Builders;
using DotNet.Testcontainers.Containers;
using DotNet.Testcontainers.Images;

public class DockerManager
{
    public static async Task<ExecResult> RunCommandWithContainer(params string[] command)
    {
		using var stdoutStream = new MemoryStream();
		using var stderrStream = new MemoryStream();
		IContainer localContainer = null;

		try
		{
			ContainerBuilder builder = new ContainerBuilder()
				// use particular version always 
				.WithImage("dnawrkshp/ps2dev-libdl:latest")
				.WithImagePullPolicy(PullPolicy.Always)
				// name it nicely
				.WithBindMount(Path.GetFullPath(Path.Combine(Environment.CurrentDirectory, FolderNames.BinaryFolder)), "/levels")
				// with some configuration
				//.WithEnvironment("DOCKER_HOST", "unix:///var/run/docker.sock")
				//.WithWaitStrategy(Wait.ForUnixContainer())
				//.WithReuse(true)
				.WithCleanUp(true)
				.WithReuse(false)
				.WithEntrypoint(command)
				.WithOutputConsumer(Consume.RedirectStdoutAndStderrToStream(stdoutStream, stderrStream));

			localContainer = builder.Build();
			await localContainer.StartAsync().ConfigureAwait(true);

			var exitCode = await localContainer.GetExitCodeAsync();
			var stdout = Encoding.UTF8.GetString(stdoutStream.ToArray());
			var stderr = Encoding.UTF8.GetString(stderrStream.ToArray());
			return new ExecResult(stdout, stderr, exitCode);
		}
		finally
		{
			if (localContainer != null)
			{
				await localContainer.DisposeAsync();
			}
		}
    }

}

#endif
