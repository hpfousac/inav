#include <stdio.h>
#include <string.h>
#include <ctype.h>


#define MAX_PWM_OUTPUT_PORTS 10 // fake
#define INT_MAX __INT_MAX__

#define white_space(c) ((c) == ' ' || (c) == '\t')

int sl_isalnum(int c);
int sl_isdigit(int c);
int sl_isupper(int c);
int sl_islower(int c);
int sl_tolower(int c);
int sl_toupper(int c);

int sl_strcasecmp(const char * s1, const char * s2);
int sl_strncasecmp(const char * s1, const char * s2, int n);

int sl_isalnum(int c)
{
    return sl_isdigit(c) || sl_isupper(c) || sl_islower(c);
}

int sl_isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int sl_isupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

int sl_islower(int c)
{
    return (c >= 'a' && c <= 'z');
}

int sl_tolower(int c)
{
    return sl_isupper(c) ? (c) - 'A' + 'a' : c;
}

int sl_toupper(int c)
{
    return sl_islower(c) ? (c) - 'a' + 'A' : c;
}

int sl_strcasecmp(const char * s1, const char * s2)
{
    return sl_strncasecmp(s1, s2, INT_MAX);
}

int sl_strncasecmp(const char * s1, const char * s2, int n)
{
    const unsigned char * ucs1 = (const unsigned char *) s1;
    const unsigned char * ucs2 = (const unsigned char *) s2;

    int d = 0;

    for ( ; n != 0; n--) {
        const int c1 = sl_tolower(*ucs1++);
        const int c2 = sl_tolower(*ucs2++);
        if (((d = c1 - c2) != 0) || (c2 == '\0')) {
            break;
        }
    }

    return d;
}

static const char *nextArg(const char *currentArg)
{
    printf ("*DEBUG* nextArg (%s);\n", currentArg); fflush (stdout);

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

static int isNumStr (char *s)
{
    while (!white_space(*s)) {
        if (!(*s >= '0' && *s <= '9')) {
            return -1;
        }
        ++s;
    }
    return 0;
}

static void inline cliPwmMapSetPpm (pwmindex)
{

}

static void inline cliPwmMapSetPwm (pwmindex, pwmtargetindex)
{

}

static void inline cliPwmMapSetServo (pwmindex, pwmtargetindex)
{

}

static void inline cliPwmMapSetMotor (pwmindex, pwmtargetindex)
{

}

static void inline cliPwmMapSetLed (pwmindex)
{

}

static void inline cliPwmMapSetBeeper (pwmindex)
{
    
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

            if (0 != isNumStr (ptr)) {
                printf ("*ERROR* pwmindex should be a number\n");
                return;
            }
            pwmindex = fastA2I(ptr);
            if (pwmindex > MAX_PWM_OUTPUT_PORTS) {
                printf ("*ERROR* pwmindex must not be higer than %d\n", MAX_PWM_OUTPUT_PORTS);
                // cliShowArgumentRangeError("pwmindex", 1, MAX_PWM_OUTPUT_PORTS);
            }

            ptr = nextArg(ptr);
            if (ptr) {
                pwmtarget = ptr;
            }

            ptr = nextArg(ptr);
            if (NULL != ptr) {
                if (0 != isNumStr (ptr)) {
                    printf ("*ERROR* pwmindex should be a number\n");
                    return;
                }
                if (ptr) {
                    pwmtargetindex = fastA2I(ptr);
                }
            } else {
                pwmtargetindex = -1;
            }

            if (!sl_strncasecmp(pwmtarget, "ppm", 3)) {
                cliPwmMapSetPpm (pwmindex);
            } else if (!sl_strncasecmp(pwmtarget, "pwm", 3)) {
                cliPwmMapSetPwm (pwmindex, pwmtargetindex);
            } else if (!sl_strncasecmp(pwmtarget, "servo", 5)) {
                cliPwmMapSetServo (pwmindex, pwmtargetindex);
            } else if (!sl_strncasecmp(pwmtarget, "motor", 5)) {
                cliPwmMapSetMotor (pwmindex, pwmtargetindex);
            } else if (!sl_strncasecmp(pwmtarget, "led", 3)) {
                cliPwmMapSetLed (pwmindex);
            } else if (!sl_strncasecmp(pwmtarget, "beeper", 6)) {
                cliPwmMapSetBeeper (pwmindex);

            } else {
                printf("*ERROR* (unknown terget pwmtarget=%s\n", pwmtarget);
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
    cliPwmMapSet("30 servo 1");
//    cliPwmMapSet("  2     servo     1");

    cliPwmMapSet("1 ppm");

    return 0;
}