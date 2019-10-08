__attribute__((noinline)) void bar()
{
    asm volatile("");
}

__attribute__((noinline)) void foo()
{
    asm volatile("");
}

int main()
{
    while (true)
    {
        foo();
        bar();
    }
}
