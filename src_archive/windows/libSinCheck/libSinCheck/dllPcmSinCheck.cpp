// libSinCheck.cpp : 定义 DLL 应用程序的导出函数。
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dllPcmSinCheck.h"
#include "include/SinAudioCheck.h"

#pragma comment(lib, "legacy_stdio_definitions.lib") 

int dllPcmSinCheck()
{
	int retarr[8] = { 0 };
	int *data_buf;
	FILE *fp_in = NULL;
	int length;
	//int i = 0;
	SinAudioCheck *audioCheck = new SinAudioCheck();
	printf("SinAudioCheck");
	audioCheck->init(1, 400);
	
	fopen_s(&fp_in, "", "rb");
	if (!fp_in) {
		printf("open file error \n");
		return -1;
	}
	
	fseek(fp_in, 0, SEEK_END);
	length = ftell(fp_in);
	fseek(fp_in, 0, SEEK_SET);
	data_buf = new int[length / sizeof(int)];
	fread(data_buf, sizeof(int), length / sizeof(int), fp_in);
	audioCheck->checkAgreement(data_buf, length / sizeof(int), retarr);
	/*for (i = 0; i < 8; i++)
	{
		if (retarr[i] != 0)
		{
			printf("channel %d error at frame %d (%d sample point)\n", i);
		}
	}*/
	audioCheck->uninit();
	delete audioCheck;
	delete[] data_buf;
	return 1;
}
