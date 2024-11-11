#include <stdio.h>

int main(int argc, char *argv[])
{
char  escape = 0x1b;
printf("%c[43mthis is a test\nmore test%c[m\n", escape, escape);
printf("%c[7mreverse video%c[mback to normal\n", escape, escape);
}

