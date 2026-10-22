class SinAudioCheck
{
	private:
	int frequency;
	int audiotype;
public:
	int init(int type,int frq);//type 0表示4路做fft  ; type为1表示8路，要分两次做fft 
	int checkNoise(int* indata, int length, int* retarray,int threshold); //type 为0时 每次传4*256*sizeof(int) ;type 为1时 每次传4*256*sizeof(int)*2
	int checkAgreement(int* inarr, int length, int* retarr);
	int uninit();
};

