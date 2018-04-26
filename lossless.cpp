// In file lossless.h
// tracking code: GWI0B
// added one file
#include "lossless.h"

long readlss::lss_info(char *fname){

	unsigned long l_temp;

	flss = fopen(fname,"rb");
	
	if(flss == NULL)
		return -1;
	
	fseek(flss,4,SEEK_SET);
	fread(&header_size, sizeof(long), 1, flss);	// header size read
	fread(ColorFormat, sizeof(char), 4, flss);	// color format read
	fread(&m_width, sizeof(short), 1, flss);		// dimx read
	fread(&m_height, sizeof(short), 1, flss);	// dimy read
	fread(&l_temp, sizeof(long), 1, flss);		// fps, frame_num
	fps = (char)(l_temp >> 24);					// frame rate
	num_frame = (long)(l_temp & 0x00ffffff);	// number of frame

	m_width <<= 1;
	
	return 1;
}

long readlss::initialize_lss_read(char *fname){

	long i;
	unsigned long l_temp;

	flss = fopen(fname,"rb");
	
	if(flss == NULL)
		return -1;

	// header read
	fseek(flss,4,SEEK_SET);
	fread(&header_size, sizeof(long), 1, flss);	// header size read
	fread(ColorFormat, sizeof(char), 4, flss);	// color format read
	fread(&m_width, sizeof(short), 1, flss);		// dimx read
	fread(&m_height, sizeof(short), 1, flss);	// dimy read
	fread(&l_temp, sizeof(long), 1, flss);		// fps, frame_num
	fps = (char)(l_temp >> 24);					// frame rate
	num_frame = (long)(l_temp & 0x00ffffff);	// number of frame

	// header allocation
	offset = (long *)malloc(sizeof(long)*num_frame*2);
	length = offset + num_frame;

	for(i=0; i<num_frame; i++){
		fread(offset + i, sizeof(long), 1, flss);	// offset saving
		fread(length + i, sizeof(long), 1, flss);	// length saving
	}

	// additional information setting
	m_width <<= 1;
	block_x = 8;
	block_y = 4;
	frame_size = m_width*m_height;
	
	curr_frame_index = -1;

	buffer = (byte *)malloc(sizeof(byte)*(frame_size<<2));
	curr_frame = buffer;					// size: frame_size
	diff_frame = curr_frame + frame_size;	// size: frame_size
	file_buffer = diff_frame + frame_size;	// size: frame_size*2

	return 1;
}

// close the lossless file
long readlss::finalize_lss_read(){
	free(offset);
	free(buffer);
	fclose(flss);

	return 1;
}

// read one frame
long readlss::read_oneframe_uyvy_lss(byte *output_frame, long frame_index){

	long i;
	long frame_file_size;
	long IntraFlag;

	if(frame_index>=num_frame)
		return -1;

	// reverse condition
	if((frame_index<curr_frame_index)||curr_frame_index<0){
		curr_frame_index = -1;
		fseek(flss,header_size,SEEK_SET);
	}

		
	while(frame_index != curr_frame_index){	

		// IntraFlag extraction 
		IntraFlag = 0x80000000&length[curr_frame_index+1];
		frame_file_size = length[curr_frame_index+1]&0x7fffffff;

		if(IntraFlag){
			fread(file_buffer,sizeof(byte),frame_file_size,flss);
			decoding_intra_frame(curr_frame);
		}
		else{
			fread(file_buffer,sizeof(byte),frame_file_size,flss);
			decoding_diff_frame(diff_frame);

			// recover original signal
			for(i=0; i<frame_size; i++)
				curr_frame[i] += diff_frame[i];
		}
		curr_frame_index++;
	}

	// output saving
	memcpy(output_frame,curr_frame,frame_size);

	return 1;
}

// look up tables
static unsigned short decode_mask[8][8] = {	{0x8000,0xc000,0xe000,0xf000,0xf800,0xfc00,0xfe00,0xff00},
											{0x4000,0x6000,0x7000,0x7800,0x7c00,0x7e00,0x7f00,0x7f80},
											{0x2000,0x3000,0x3800,0x3c00,0x3e00,0x3f00,0x3f80,0x3fc0},
											{0x1000,0x1800,0x1c00,0x1e00,0x1f00,0x1f80,0x1fc0,0x1fe0},
											{0x0800,0x0c00,0x0e00,0x0f00,0x0f80,0x0fc0,0x0fe0,0x0ff0},
											{0x0400,0x0600,0x0700,0x0780,0x07c0,0x07e0,0x07f0,0x07f8},
											{0x0200,0x0300,0x0380,0x03c0,0x03e0,0x03f0,0x03f8,0x03fc},
											{0x0100,0x0180,0x01c0,0x01e0,0x01f0,0x01f8,0x01fc,0x01fe}	};

static byte decode_divider[8][8] = {	{0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08},
										{0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07},
										{0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06},
										{0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05},
										{0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04},
										{0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03},
										{0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02},
										{0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01}	};

static byte decode_look_up0[]={	0x00,0xff };
static byte decode_look_up1[]={	0x00,0x01,0xfe,0xff };
static byte decode_look_up2[]={	0x00,0x01,0x02,0x03,0xfc,0xfd,0xfe,0xff };
static byte decode_look_up3[]={	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff };
static byte decode_look_up4[]={	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
								0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff };
static byte decode_look_up5[]={	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
								0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
								0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
								0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff };
static byte decode_look_up6[]={	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
								0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
								0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
								0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
								0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
								0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
								0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
								0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff };
static byte decode_look_up7[]={	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
								0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
								0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
								0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
								0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
								0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
								0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
								0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
								0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
								0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
								0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
								0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
								0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
								0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
								0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
								0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff };
byte *decode_look_up[8] = { decode_look_up0, decode_look_up1, decode_look_up2, decode_look_up3, 
							decode_look_up4, decode_look_up5, decode_look_up6, decode_look_up7 };

void readlss::decoding_diff_frame(byte *diff_frame){

	long i,j,k,l;
	long width=m_width,height=m_height;
	long BlkShiftY=width*block_y;
	long gPosition,lPosition;
	long BitCnt=0;
	byte BitLength,BitLengthReal;
	// temporal variables
	unsigned short sTemp;

	// temporal pointer for difference frame (input buffer)
	byte *py_diff,*px_diff,*pline;

	// process all image space
	for(i=0,py_diff=diff_frame;i<height;i+=block_y,py_diff+=BlkShiftY){
		for(j=0,px_diff=py_diff;j<width;j+=block_x,px_diff+=block_x){

			// get bit length
			gPosition = BitCnt>>3, lPosition = BitCnt%8;		// find out global and local position
			sTemp = file_buffer[gPosition], sTemp<<=8, sTemp |= file_buffer[gPosition+1];	// read to temporal variable
			sTemp &= decode_mask[lPosition][2];				// decode bit length using table
			sTemp >>= decode_divider[lPosition][2];			// decode bit length using table
			BitLength = (byte)sTemp;			// calculate Bit length
			BitLengthReal = BitLength + 1;		// calculate real bit length
			BitCnt += 3;

			// block operation
			for(k=0,pline=px_diff;k<block_y;k++,pline+=width){
				if(i+k>=height)	break;		// condition for out of bound

				for(l=0;l<block_x;l++){
					if(j+l>=width)	break;	// condition for out of bound
					gPosition = BitCnt>>3, lPosition = BitCnt%8;		// find out global and local position
					sTemp = file_buffer[gPosition], sTemp<<=8, sTemp |= file_buffer[gPosition+1];	// read to temporal variable
					sTemp &= decode_mask[lPosition][BitLength];			// decode value using table
					sTemp >>= decode_divider[lPosition][BitLength];		// decode value using table
					pline[l] = decode_look_up[BitLength][sTemp];		// decode value using table
					BitCnt += BitLengthReal;
				}
			}
		}
	}
}

void readlss::decoding_intra_frame(byte *output_frame){

	long i,j,k,l;
	long width=m_width,height=m_height;
	long BlkShiftY=width*block_y;
	long gPosition,lPosition;
	long BitCnt=0;
	byte BitLength,BitLengthReal;
	byte RefBlockIdx;
	// temporal variables
	unsigned short sTemp;

	// temporal pointer for difference frame (input buffer)
	byte *py_diff,*px_diff,*pline,*plineRef=0;

	// for all image space
	for(i=0,py_diff=output_frame;i<height;i+=block_y,py_diff+=BlkShiftY){
		for(j=0,px_diff=py_diff;j<width;j+=block_x,px_diff+=block_x){

			// get reference block index
			gPosition = BitCnt>>3, lPosition = BitCnt%8;		// find out global and local position
			sTemp = file_buffer[gPosition], sTemp<<=8, sTemp |= file_buffer[gPosition+1];	// read to temporal variable
			sTemp &= decode_mask[lPosition][1];				// decode bit length using table
			sTemp >>= decode_divider[lPosition][1];			// decode bit length using table
			RefBlockIdx = (byte)sTemp;			// Save reference block index
			BitCnt += 2;

			// get bit length
			gPosition = BitCnt>>3, lPosition = BitCnt%8;		// find out global and local position
			sTemp = file_buffer[gPosition], sTemp<<=8, sTemp |= file_buffer[gPosition+1];	// read to temporal variable
			sTemp &= decode_mask[lPosition][2];				// decode bit length using table
			sTemp >>= decode_divider[lPosition][2];			// decode bit length using table
			BitLength = (byte)sTemp;			// calculate Bit length
			BitLengthReal = BitLength + 1;		// calculate real bit length
			BitCnt += 3;

			// not block itself
			if(RefBlockIdx){

				// allocate reference block index
				switch(RefBlockIdx){
				case 1: // upper-left case
					plineRef = px_diff - BlkShiftY - block_x;
					break;
				case 2: // upper case
					plineRef = px_diff - BlkShiftY;
					break;
				case 3: // left case
					plineRef = px_diff - block_x;
					break;
				}

				// block operation
				for(k=0,pline=px_diff;k<block_y;k++,pline+=width,plineRef+=width){
					if(i+k>=height)	break;		// condition for out of bound

					for(l=0;l<block_x;l++){
						if(j+l>=width)	break;	// condition for out of bound
						gPosition = BitCnt>>3, lPosition = BitCnt%8;		// find out global and local position
						sTemp = file_buffer[gPosition], sTemp<<=8, sTemp |= file_buffer[gPosition+1];	// read to temporal variable
						sTemp &= decode_mask[lPosition][BitLength];					// decode value using table
						sTemp >>= decode_divider[lPosition][BitLength];				// decode value using table
						pline[l] = plineRef[l] + decode_look_up[BitLength][sTemp];	// decode value using table
						BitCnt += BitLengthReal;
					}
				}
			}
			// block itself
			else{
				// block operation
				for(k=0,pline=px_diff;k<block_y;k++,pline+=width){
					if(i+k>=height)	break;		// condition for out of bound

					for(l=0;l<block_x;l++){
						if(j+l>=width)	break;	// condition for out of bound
						gPosition = BitCnt>>3, lPosition = BitCnt%8;		// find out global and local position
						sTemp = file_buffer[gPosition], sTemp<<=8, sTemp |= file_buffer[gPosition+1];	// read to temporal variable
						sTemp &= decode_mask[lPosition][BitLength];					// decode value using table
						sTemp >>= decode_divider[lPosition][BitLength];				// decode value using table
						pline[l] = decode_look_up[BitLength][sTemp];	// decode value using table
						BitCnt += BitLengthReal;
					}
				}
			}
		}
	}
}	