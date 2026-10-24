# Container.cpp

Linux container from scratch on MacOS silicon.

## Containers in a Nutshell

Containers are processes!

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
