

// concentration unit for PM data
typedef struct {
	uint16_t	PM1_0;	// PM1.0 concentration unit μ g/m3
	uint16_t	PM2_5;	// PM2.5 concentration unit μ g/m3
	uint16_t	PM10;	// PM10 concentration unit μ g/m3
} _concentration_unit_t;

// PMS7003 transport protocol-Active Mode : 32 Bytes
typedef struct {
	unsigned char			frame_header[2];	// Fixed : start char 1 [0x42] + start char 2 [0x4d]
	uint16_t				frame_len;			// 2 BYTE : Frame length=2x13+2(data+check bytes)
	_concentration_unit_t	standard_particle;	// CF=1，standard particle
	_concentration_unit_t	atmospheric_env;	// under atmospheric environment
	uint16_t				checksum;			// 2 BYTE : Check code=Start character 1+ Start character 2+……..+data 13 Low 8 bits
} _pms7003_protocol_t;
