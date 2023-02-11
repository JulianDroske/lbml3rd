# lbml3rd
### version 0.9

#### Lightweight boot manager under Linux.

Normally, a bootloader/boot manager is loaded to decide which kernel to load.
But what if a kernel is loaded by bootloader first?
Like, on ARM Chromebooks or many Android phones,
you may have no choice but to use the stock kernel, so multibooting linux distros could be hard.

lbml3rd does the same as MultiROM -- but more generally, not only on Android phones.
It is only an `init` executable file so you can load it in any situation under linux, such as when `PID != 1`.
It does not depend on anything under rootfs, the final binary file `linit` is statically linked with musl.

It can use native console (mostly /dev/tty0) and framebuffer (mostly /dev/fb0) to show contents and shell.
You can modify `/lbml.toml` to customize your boot config.

## Build/Compile

```shell
./make.sh
```

This will generate `linit`, which can replace /sbin/init or /init in initramfs.

Full usage:

```shell
./make.sh [target|"clean"]
```
where: target can be "aarch64", "x86_64" or anything as a multiarch name.

Note: For `arm` (32-bit) target, change `gnu` to `gnueabi` on line 5 in `make.sh`.

lbml3rd supports static config which does not rely on `/lbml.toml`.
To do so, copy your config file to `./lbml_static_config.toml` and run the compile script,
it will auto find the file and use it.

clean:
```shell
./make.sh clean
```

## Usage

After compiling the source code, copy `linit` to somewhere and add `init=$path` to kernel boot parameters,
where `$path` is the location of `linit`.

For Android boot.img, you can either replace `/init` in ramdisk or change the kernel boot parameters.

For Chromebooks, use [Chromebook Kernel Utils](https://github.com/drsn0w/chromebook_kernel_tools)
to unpack and repack the kernel known as `KERN-A` in ChromeOS partition table.

You can use it to test screen drivers and boot process, or just as a normal boot manager.

An example config file is `./example.toml`.

After entering boot menu,
Volume-, Volume+, Up, Down to select boot option, Power or Enter to boot selected option.
R to reboot, C to enter sash (a simple shell).

## Screenshots

#### Splash (550C)

![](docs/splash_550c.png)

#### Splash (jurt)

![](docs/splash_jurt.png)

#### Menu

![](docs/menu.png)

#### sash
![](docs/sash.png)

## Notes

Entry type "shell" and "kexec" are not implemented yet, only "init" can be used.

Currently it does not support MTK phones (or some Android phones? like redmi10x(atom)) for unknown reason as display does not work.
Until I find some way to let them work, it should be fine on most MSM phones (like Redmi2(wt88047) and Mi6)
and Chromebooks (stock kernel not tested).

`musl`, `zlib`, `libfbpads`, `libudev-zero` and `sash` are snapshots.

portable libs integrated: `toml`, `stb_image`, `jurt(2016)`

`sash.c` under dir `sash` is modified to let lbml3rd work properly.

