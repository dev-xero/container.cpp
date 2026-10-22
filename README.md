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
sudo apk add g++ make musl-dev cmake
```

## Environment

Clone

Make dir writable in Lima!!

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
