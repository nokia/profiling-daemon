#!/usr/bin/env python3
import asyncio
import os
import time


class Qemu:
    def __init__(self, hda, hdb):
        self._hda = hda
        self._hdb = hdb

    async def readline(self):
        return await self._qemu.stdout.readline()

    async def write(self, data):
        self._qemu.stdin.write(data)

    async def __aenter__(self):
        await self._start()
        return self

    async def __aexit__(self, exc_type, exc, tb):
        print('terminating qemu')
        self._qemu.terminate()
        await self._qemu.wait()

    async def _start(self):
        self._qemu = await asyncio.create_subprocess_shell(
            f'qemu-system-x86_64 -serial stdio -hda {self._hda} -hdb {self._hdb}',
            stdout=asyncio.subprocess.PIPE, stdin=asyncio.subprocess.PIPE)

        await self._wait_for_login_screen()
        await asyncio.sleep(0.3)
        print('got login screen')

        await self._login()
        print('logged in')

    async def _wait_for_login_screen(self):
        while True:
            out = await self._qemu.stdout.readline()
            if out.decode('utf-8').startswith('Debian GNU/Linux'):
                break

    async def _login(self):
        self._qemu.stdin.write(b'root\r')
        await asyncio.sleep(0.3)
        self._qemu.stdin.write(b'root\r')
        await asyncio.sleep(1)


class ShellError(Exception):
    pass


async def _shell(cmd):
    shell = await asyncio.create_subprocess_shell(cmd)
    return_code = await shell.wait()
    if return_code != 0:
        raise ShellError(cmd)


async def download_qemu_image():
    qemu_image='debian_squeeze_amd64_standard.qcow2'
    qemu_image_url=f'https://people.debian.org/~aurel32/qemu/amd64/{qemu_image}'

    if not os.path.exists(qemu_image):
        print(f'getting {qemu_image}')
        wget = await asyncio.create_subprocess_shell(f'wget {qemu_image_url}')
        await wget.wait()

    return qemu_image


async def prepare_profd_image(profd_binary):
    profd_image = 'profd.ext4'
    await _shell(f'dd if=/dev/zero of={profd_image} bs=1M count=50')
    await _shell(f'mkfs.ext3 {profd_image}')
    await _shell(f'e2cp {profd_binary} {profd_image}:/')
    return profd_image


async def main():
    profd_image = await prepare_profd_image('./build/poor-perf')
    print(f'profd packed in {profd_image}')

    qemu_image = await download_qemu_image()
    print(f'got {qemu_image}')

    async with Qemu(qemu_image, profd_image) as qemu:
        await qemu.write(b'echo hello world\r')
        while True:
            out = await qemu.readline()
            if out:
                print(out)


if __name__ == '__main__':
    asyncio.run(main())
