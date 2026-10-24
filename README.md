# Container.cpp

```
                __       _
 _______  ___  / /____ _(_)__  ___ ____ _______  ___
/ __/ _ \/ _ \/ __/ _ `/ / _ \/ -_) __// __/ _ \/ _ \
\__/\___/_//_/\__/\_,_/_/_//_/\__/_/ (_)__/ .__/ .__/
                                         /_/  /_/
```

This is my attempt at implementing a crude Linux container from scratch in a Virtual Machine on MacOS/arm64.

## Table of Contents

- [Containers in a Nutshell](#containers-in-a-nutshell)
- [Bare Minimum Objectives](#bare-minimum-objectives)
- [Virtual Machine Setup](#virtual-machine-setup)
    - [Install Linux Machines (Lima)](#install-linux-machines-lima)
    - [Alpine Linux](#alpine-linux)

## Containers in a Nutshell

A container at the most fundamental level on Linux systems is just another [process](<https://en.wikipedia.org/wiki/Process_(computing)>). A process is an instance of a running program.

Now then, what makes containers different from regular processes? You could say that a container has a much more 'contained' and controlled view of the system's resources. This is typically achieved by what's known as [Namespaces](https://en.wikipedia.org/wiki/Linux_namespaces). Namespaces, aptly named, control what resources a process can 'see'. The second half of the puzzle are [Control Groups](https://en.wikipedia.org/wiki/Cgroups). Where namespaces decided what a process can see, Control Groups (or cgroups) decide how much system resources the said process can use.

By controlling what system resources a process can see and use, we create the illusion that allows containers to behave as isolated units on the same host kernel.

## Bare Minimum Objectives

To make it easy to stay on track. I'm going to define the bare minimum the program must accomplish in order for us to consider its process a container.

1. Spawn a process given a command of the form `./container run <cmd> --fs=<fs-path>`
2. Ensure that this process has its own namespace (pid, user, mount)
3. Ensure that the root mount point is isolated from the parent process
4. Enforce memory and CPU limits using cgroups

The reason we specify a `--fs` or file system flag will become apparent later. The same goes for why we need mount point isolation.

## Virtual Machine Setup

> **Note:** You can skip this step if you're running Linux already.

From here on out, I assume that you're on an arm64 processor, specifically an Apple Silicon. Changing these steps for x86 shouldn't be difficult. If you're unsure what cpu architecture your computer is using, run `uname -m`. I'm going to make another assumption that you have [homebrew](https://brew.sh/) installed as well.

### Install Linux Machines (Lima)

Lima will help setup a light weight Linux Virtual Machine on your computer.

```bash
brew install lima
limactl start
```

> **Note:** On first run, Lima will ask you to download Ubuntu. I've only experimented on Alpine Linux and the following steps will assume you're running an Alpine box.

### Alpine Linux

Alpine Linux is an extremely lightweight Linux distro commonly used in embedded environments (see [busybox](https://en.wikipedia.org/wiki/BusyBox)). I've chosen this distro because it's simple to use. However, because it's so lightweight we're missing a few tools and binaries, namely some C++ build tools.

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
- https://man7.org/linux/man-pages/man7/namespaces.7.html
- https://man7.org/linux/man-pages/man7/cgroups.7.html
- https://linux.die.net/man/2/setgroups
- https://linux.die.net/man/3/uid
- http://man7.org/linux/man-pages/man2/clone.2.html
- https://man7.org/linux/man-pages/man3/sysconf.3.html
