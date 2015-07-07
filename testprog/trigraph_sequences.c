/* 三字符序列(trigraph sequences)测试 */
/* 下面的??=代表# */
??=include"included_files1.h"
/* 下面的??(代表[ , ??)代表] */
int a??(5??);
struct pair { int a; int b; }
struct pair pa = ??<0??>; /* ??< 和??> 分别代表{和} */
int var1 = 2??'1; /* ??'代表^ 是按位异或的意思 */
int var2 = 1??!2; /* ??!代表| 是按位或的意思 */
int var3 = ??-7;  /* ??-代表~ 是按位取反的意思 */
char *string1 = "hello??/tworld"; /* ??/是\的意思,和后面的t合起来就是\t */
