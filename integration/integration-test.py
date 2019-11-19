#!/usr/bin/env python3
import asyncio
import os
import time


class Qemu:
    def __init__(self, image):
        self._image = image

    async def readline(self):
        return await qemu.stdout.readline()

    async def write(self, data):
        self._qemu.stdin.write(data)

    async def __aenter__(self):
        await self._start()

    async def __aexit__(self, exc_type, exc, tb):
        await self._qemu.terminate()
        await self._qemu.wait()

    async def _start(self):
        self._qemu = await asyncio.create_subprocess_shell(
            f'qemu-system-x86_64 -serial stdio {self._image}', stdout=asyncio.subprocess.PIPE, stdin=asyncio.subprocess.PIPE)

        await self._wait_for_login_screen()
        print('got login screen')

        time.sleep(0.3)

        await self._login()
        print('logged in')

        self._qemu.stdin.write(b'echo hello world\r')

        while True:
            out = await self._qemu.stdout.readline()
            if out:
                print(out)

    async def _wait_for_login_screen(self):
        while True:
            out = await self._qemu.stdout.readline()
            if out.decode('utf-8').startswith('Debian GNU/Linux'):
                break

    async def _login(self):
        self._qemu.stdin.write(b'root\r')
        time.sleep(0.3)
        self._qemu.stdin.write(b'root\r')
        time.sleep(1)


async def main():
    qemu_image='debian_squeeze_amd64_standard.qcow2'
    qemu_image_url=f'https://people.debian.org/~aurel32/qemu/amd64/{qemu_image}'

    if not os.path.exists(qemu_image):
        print(f'getting {qemu_image}')
        wget = await asyncio.create_subprocess_shell(f'wget {qemu_image_url}')
        await wget.wait()

    print(f'got {qemu_image}')

    async with Qemu(qemu_image) as qemu:
        qemu.write(b'echo hello world\r')
        while True:
            out = await qemu.readline()
            if out:
                print(out)


if __name__ == '__main__':
    asyncio.run(main())
