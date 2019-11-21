#!/usr/bin/env python3
import asyncio
import os
import time


class Qemu:
    def __init__(self, hda, hdb):
        self._hda = hda
        self._hdb = hdb

    async def readline(self):
        out = await self._qemu.stdout.readline()
        self._print(out)
        return out

    async def readuntil(self, separator):
        out = await self._qemu.stdout.readuntil(separator)
        self._print(out)
        return out

    async def wait_for_prompt(self):
        await self.readuntil(b'root@debian-amd64:~#')

    def write(self, data):
        self._qemu.stdin.write(data)

    def _print(self, b):
        for line in b.decode('utf-8').splitlines():
            print(f'qemu> {line}')

    async def __aenter__(self):
        await self._start()
        return self

    async def __aexit__(self, exc_type, exc, tb):
        print('terminating qemu')
        self._qemu.terminate()
        await self._qemu.communicate()

    async def _start(self):
        self._qemu = await asyncio.create_subprocess_exec(
            'qemu-system-x86_64', '-serial', 'stdio', '-hda', self._hda, '-hdb', self._hdb,
            stdout=asyncio.subprocess.PIPE, stdin=asyncio.subprocess.PIPE)

        await self.readuntil(b'login: ')
        self.write(b'root\r')

        await self.readuntil(b'Password: ')
        self.write(b'root\r')

        await self.wait_for_prompt()
        print('logged in')


class ShellError(Exception):
    pass


async def _shell(cmd):
    shell = await asyncio.create_subprocess_shell(cmd)
    return_code = await shell.wait()
    if return_code != 0:
        raise ShellError(cmd)


async def download_qemu_image():
    debian_image='debian_squeeze_amd64_standard.qcow2'
    qemu_image_url=f'https://people.debian.org/~aurel32/qemu/amd64/{debian_image}'

    if not os.path.exists(debian_image):
        print(f'getting {debian_image}')
        wget = await asyncio.create_subprocess_shell(f'wget {qemu_image_url}')
        await wget.wait()

    fresh_image = 'testing.qcow2'
    await _shell(f'cp {debian_image} {fresh_image}')

    return fresh_image


async def prepare_profd_image(profd_binary):
    profd_image = 'profd.ext4'
    await _shell(f'dd if=/dev/zero of={profd_image} bs=1M count=50')
    await _shell(f'mkfs.ext3 {profd_image}')
    await _shell(f'e2cp {profd_binary} {profd_image}:/')
    return profd_image


async def mount_profd_image(qemu):
    qemu.write(b'mkdir /profd ; mount /dev/sdb /profd ; chmod +x /profd/poor-perf ; echo ok\r')
    await qemu.wait_for_prompt()
    print('mounted profd image')


async def main():
    profd_image = await prepare_profd_image('./build/poor-perf')
    print(f'profd packed in {profd_image}')

    qemu_image = await download_qemu_image()
    print(f'got {qemu_image}')

    async with Qemu(qemu_image, profd_image) as qemu:
        qemu.write(b'echo hello world\r')
        await qemu.wait_for_prompt()

        await mount_profd_image(qemu)

        print('running profd')
        qemu.write(b'/profd/poor-perf --mode oneshot --duration 10 --output /profd/profile.txt\r')
        await qemu.wait_for_prompt()


if __name__ == '__main__':
    asyncio.run(main())
