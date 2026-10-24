# Container.cpp

```
 _______  ___  / /____ _(_)__  ___ _______  _______  ___ 
/ __/ _ \/ _ \/ __/ _ `/ / _ \/ -_) __(_-<_/ __/ _ \/ _ \
\__/\___/_//_/\__/\_,_/_/_//_/\__/_/ /___(_)__/ .__/ .__/
                                             /_/  /_/    
```

This is my attempt at implementing a crude Linux 'container' from scratch on MacOS (arm64).

## Table of Contents

- [Containers in a Nutshell](#containers-in-a-nutshell)
- [Bare Minimum Objectives](#bare-minimum-objectives)
- [Virtual Machine Setup](#containers-in-a-nutshell)

## Containers in a Nutshell

A container at the most fundamental level on Linux systems is just another [process](<https://en.wikipedia.org/wiki/Process_(computing)>). A process is an instance of a running program.

Now then, what makes containers different from regular processes? You could say that a container has a much more 'contained' and controlled view of the system's resources. This is typically achieved by what's known as [Namespaces](https://en.wikipedia.org/wiki/Linux_namespaces). Namespaces, aptly named, control what resources a process can 'see'. The second half of the puzzle are [Control Groups](https://en.wikipedia.org/wiki/Cgroups). Where namespaces decided what a process can see, Control Groups (or cgroups) decide how much system resources the said process can use.

By controlling what system resources a process can see and use, we create the illusion that allows containers to behave as isolated units inside the same host.

## Bare Minimum Objectives

To make it easy to stay on track. I'm going to define the bare minimum the program must accomplish in order for us to consider its process a container.

1. Spawn a process given a command of the form `./container run <cmd> --fs=<fs-path>`
2. Ensure that this process has its own namespace, process identifier (pid), and user
3. Ensure that the root mount point is isolated from the parent process

The reason we specify a `--fs` or file system flag will become apparent later. The same goes for why we need mount point isolation.

## Virtual Machine Setup

Setup your Virtual Machine (VM) - lima

Using Alpine Linux

Install C++ build tools

```bash
sudo apk update
sudo apk add g++ procps make musl-dev cmake
```

## Environment Setup

Clone

Make dir writable in Lima!!

### Alpine Linux MiniFS

I've downloaded the alpine mini root filesystem build for my particular processor architecture (aarch64). If you're not sure of yours run `uname -m` in the lima shell.

```bash
limactl shell default
sudo mkdir -p /opt/alpine-rootfs
wget https://dl-cdn.alpinelinux.org/alpine/latest-stable/releases/aarch64/alpine-minirootfs-3.24.0-aarch64.tar.gz
sudo tar -xzf alpine-minirootfs-3.24.0-aarch64.tar.gz -C /opt/alpine-rootfs
```

## Experimenting

Compile inside the Linux Virtual Machine

```bash
g++ -std=gnu++17 container.cpp -o container
```

Try running something

```bash
./container run /bin/bash
$ hostname
```

You should see 'containerd' printed. This process is effectively running in its own namespace!

## Caveats

No proper error handling kill on failure

## References

- https://youtu.be/8fi7uSYlOdc
- https://en.wikipedia.org/wiki/Linux_namespaces
- https://en.wikipedia.org/wiki/Cgroups
- https://en.wikipedia.org/wiki/BusyBox
- https://en.wikipedia.org/wiki/Systemd
- https://lwn.net/Articles/689856
- https://man7.org/linux/man-pages/man2/wait.2.html
- https://linux.die.net/man/2/setgroups
- https://linux.die.net/man/3/uid
- http://man7.org/linux/man-pages/man2/clone.2.html
- https://man7.org/linux/man-pages/man3/sysconf.3.html
