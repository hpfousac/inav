#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define white_space(c) ((c) == ' ' || (c) == '\t')

static const char *nextArg(const char *currentArg)
{
    printf ("*DEBUG* nextArg %s\n", currentArg); fflush (stdout);

    const char *ptr = strchr(currentArg, ' ');
    while (ptr && white_space(*ptr)) {
        ptr++;
    }

    printf ("\treturn=%s\n", ptr); fflush (stdout);
    return ptr;
}

int a2d(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    else
        return -1;
}

int fastA2I(const char *s)
{
    int sign = 1;
    int num = 0;
    int digit;

    while (white_space(*s)) {
        s++;
    }

    if (*s == '-') {
        sign = -1;
        s++;
    }

    while ((digit = a2d(*s)) >= 0) {
        if (digit > 10)
            break;
        num = num * 10 + digit;
        s++;
    }

    return sign * num;
}

static void cliPwmMapSet(char *cmdline)
{
    printf ("*DEBUG* pwmmap %s\n", cmdline); fflush (stdout);
    
        char 
            *ptr = cmdline, 
            *pwmtarget = NULL;
        int 
            pwmindex = -1,
            pwmtargetindex = -1;

            pwmindex = fastA2I(ptr);

            ptr = nextArg(ptr);
            if (ptr) {
                pwmtarget = ptr;
            }
            ptr = nextArg(ptr);
            if (ptr) {
                pwmtargetindex = fastA2I(ptr);
            }

        printf("*DEBUG* (2)pwmmap pwmindex=%d pwmtarget=%s pwmtargetindex=%d\n", pwmindex, pwmtarget, pwmtargetindex);
}

main ()
{
    cliPwmMapSet("2 servo 1");
    cliPwmMapSet("Z servo L");
    cliPwmMapSet("2l servo 1");
    cliPwmMapSet("21 servo 1");
    cliPwmMapSet("2l servo 1");
    cliPwmMapSet("2O servo 1");
//    cliPwmMapSet("  2     servo     1");
    return 0;
}