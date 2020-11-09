#include <stdio.h>
#include <stdlib.h>


int main(int argc, char**argv)
{
	FILE * pF = NULL;
	char *pBuf = NULL;
	size_t len = 0, i = 0, j = 0;
	
	
	if (argc != 3)
	{
		printf("usage: %s srcfilename dstfilename\r\n", argv[0]);
		return -1;
	}
	
	pBuf = (char *)malloc(10 * 1024);		// 申请10k个字节做缓存区
	if (NULL == pBuf)
	{
		printf("malloc error.\r\n");
		return -1;
	}
	
	pF = fopen(argv[1], "r");
	if (NULL == pF)
	{
		printf("fopen %s error.\r\n", argv[1]);
		return -1;
	}
	len = fread(pBuf, sizeof(char), 10*1024, pF);
	fclose(pF);
	
	for (i=0, j=0; i<len;)
	{
		switch (pBuf[i])
		{
			case '\r':
				pBuf[j] = ',';	i++; j++;
				break;
			case '\n':
				i++;
				break;
			default:
				pBuf[j] = pBuf[i];	i++; j++;
				break;
		}
	}
	
	pF = fopen(argv[2], "w+");
	if (NULL == pF)
	{
		printf("fopen %s error.\r\n", argv[2]);
		return -1;
	}
	
	fwrite(pBuf, sizeof(char), j, pF);
	free(pBuf);
	fclose(pF);
	
	return 0;
}