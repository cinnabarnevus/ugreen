/* Linux I2C demo */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define I2C_FILE_NAME		"/dev/i2c-1"
#define I2C_LOADER_ADDR		0x10

typedef unsigned int uint32_t;

struct i2c_message_t {
	uint32_t cmd;
	uint32_t data_w_len;
	uint32_t data_u32[1];
} __attribute__((aligned(64)));

int i2c_file;
int fw_file;

#define CMD_UPDATE_APROM            0x000000A0
#define CMD_GET_LD_VERSION          0x000000A1
#define CMD_ERASE_ALL               0x000000A2
#define CMD_ERASE_APROM             0x000000A3
#define CMD_RUN_APROM               0x000000A4
#define CMD_RUN_LDROM               0x000000A5
#define CMD_GET_AP_CHECKSUM         0x000000A6

int ld_send_mesg(struct i2c_message_t *mesg)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[1];

	messages[0].addr  = I2C_LOADER_ADDR;
	messages[0].flags = 0;
	messages[0].len   = sizeof(struct i2c_message_t);
	messages[0].buf   = (void *)mesg;

	packets.msgs  = messages;
	packets.nmsgs = 1;
	if(ioctl(i2c_file, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 1;
	}
	return 0;
}

int ld_recv_mesg(struct i2c_message_t *mesg)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[1];

	messages[0].addr  = I2C_LOADER_ADDR;
	messages[0].flags = I2C_M_RD;
	messages[0].len   = sizeof(struct i2c_message_t);
	messages[0].buf   = (void *)mesg;

	packets.msgs  = messages;
	packets.nmsgs = 1;
	if(ioctl(i2c_file, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 1;
	}
	return 0;
}

int ld_get_version(void)
{
	int ret;
	struct i2c_message_t mesg = {0};
	mesg.cmd = CMD_GET_LD_VERSION;
	mesg.data_w_len = 1;
	ret = ld_send_mesg(&mesg);
	ret = ld_recv_mesg(&mesg);
	if (ret) return -1;
	return mesg.data_u32[0];
}

uint32_t ld_get_checksum(uint32_t end_address)
{
	struct i2c_message_t mesg = {0};
	mesg.cmd = CMD_GET_AP_CHECKSUM;
	mesg.data_w_len = 1;
	mesg.data_u32[0] = end_address;
	ld_send_mesg(&mesg);
	ld_recv_mesg(&mesg);
	return mesg.data_u32[0];
}

void ld_update_fw(unsigned char *fw, int length)
{
	const int max_byte = sizeof(struct i2c_message_t) - 8;	//struct i2c_message_t -> data_u32
	unsigned char *p = fw;
	int download = 0;
	int n = 0;
	struct i2c_message_t mesg = {0};

	if (fw == NULL || length == 0 || length > 16*1024) {
		return ;
	}

	/* Erase Aprom */
	printf("Erase Aprom ...\n");
	mesg.cmd = CMD_ERASE_APROM;
	ld_send_mesg(&mesg);
	sleep(2);

	printf("Update Aprom start\n");
	printf("Update length:\t%d\n", length);
	mesg.cmd = CMD_UPDATE_APROM;
	while ( download < length ) {
		n = length - download;
		n = ( n > max_byte ) ? max_byte : n;
		memcpy(mesg.data_u32, fw + download, n);
		mesg.data_w_len = (n + 3) / 4;
		/* Update Aprom */
		ld_send_mesg(&mesg);
		printf("\rUpdate Aprom %d", download);
		usleep(5000);

		download += n;
	}
	printf("\nUpdate Aprom done\n");
}

void ld_run_aprom(void)
{
	struct i2c_message_t mesg = {0};
	mesg.cmd = CMD_RUN_APROM;
	mesg.data_w_len = 1;
	ld_send_mesg(&mesg);
	return ;
}


static uint32_t Checksum(unsigned char *buf, int len)
{
	int i;
	uint32_t c;

	for (c = 0, i = 0 ; i < len; i++)
	{
		c += buf[i];
	}

	return (c);
}

int main(int argc, char **argv)
{
	unsigned char *fw_buffer;
	int fw_length;
	int upgrade_success = 0;
	uint32_t mcu_checksum;
	uint32_t fw_checksum;
	int mcu_ld_verison;

	printf("\n");
	printf("*** Update tool for mini54 ***\n");
	printf("build time:\t%s %s\n", __DATE__, __TIME__);
	printf("i2c bus:\t%s\n", I2C_FILE_NAME);
	printf("i2c addr:\t0x%x\n", I2C_LOADER_ADDR);
	printf("\n");

	if (sizeof(struct i2c_message_t) != 64) {
		printf("The sizeof i2c_message_t should be 64bytes\n");
	}

	i2c_file = open(I2C_FILE_NAME, O_RDWR);
	if (i2c_file < 0) {
		perror("Can not open i2c node");
	}

	if (argc >= 2) {
		fw_file = open(argv[1], O_RDONLY);
	}
	if (fw_file < 0) {
		perror("Can not open firmware file");
	}

	mcu_ld_verison = ld_get_version();
	if (mcu_ld_verison < 0) {
		printf("MCU CAN NOT FIND LOADER\n");
		return -1;
	} else {
		printf("MCU LOADER VERSION:\t%d\n", mcu_ld_verison);
	}

	if (fw_file > 0) {
		fw_length = lseek(fw_file, 0, SEEK_END);
		fw_buffer = malloc( fw_length );
		lseek(fw_file, 0, SEEK_SET);

		mcu_checksum = ld_get_checksum(fw_length);
		printf("MCU CURRENT CHECKSUM:\t%x\n", mcu_checksum);

		read(fw_file, fw_buffer, fw_length);

		fw_checksum = Checksum(fw_buffer, fw_length);
		printf("NEW FIRMWARE CHECKSUM:\t%x\n", fw_checksum);
		printf("NEW FIRMWARE SIZE:\t%d\n", fw_length);

		if (mcu_checksum == fw_checksum) {
			printf("MCU already updated\n");
			upgrade_success = 1;
			goto completed;
		}

		ld_update_fw(fw_buffer, fw_length);

		mcu_checksum = ld_get_checksum(fw_length);
		printf("MCU CURRENT CHECKSUM:\t%x\n", mcu_checksum);

		if (mcu_checksum != fw_checksum) {
			printf("MCU update failed!\n");
			upgrade_success = 0;
			goto completed;
		}
		printf("MCU update success!\n");
		upgrade_success = 1;
	}

completed:
	if (upgrade_success == 1) {
		printf("MCU enter aprom\n");
		system("sync");
		printf("System will power off in 5 second!!!\n");
		sleep(5);
		ld_run_aprom();
	} else {
		;
	}

	return 0;
}
