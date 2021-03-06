// Functions required by several system calls (strings and binaries)
// TODO : Built-in gcc

#include <string.h>
#include <phantom_types.h>
#include <phantom_libc.h>
#include <stdlib.h>


long
atoln(const char *str, size_t n)
{
    char buf[80];

    if( n > (sizeof(buf) - 1) )
        n = sizeof(buf) - 1;

    strlcpy( buf, str, n );

    return(strtol(buf, (char **)0, 10));
}

int  atoin(const char *str, size_t n)
{
    return (int)atoln(str, n); 
}


char *
strnstrn(char const *s1, int l1, char const *s2, int l2)
{
	//int l1, l2;

	//l2 = strlen(s2);
	if (!l2)
		return (char *)s1;
	//l1 = strlen(s1);
	while(l1 >= l2) {
		l1--;
		if (!memcmp(s1,s2,l2))
			return (char *)s1;
		s1++;
	}
	return 0;
}

