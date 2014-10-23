/*
 * ============================================================================
 *
 *       Filename:  process.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年08月25日 14时31分55秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jianxi sun (jianxi), ycsunjane@gmail.com
 *   Organization:  
 *
 * ============================================================================
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <linux/if_ether.h>

#include "net.h"
#include "log.h"
#include "msg.h"
#include "aphash.h"
#include "arg.h"
#include "netlayer.h"
#include "apstatus.h"
#include "link.h"
#include "process.h"

char 	acuuid[UUID_LEN];

static void __get_cmd_stdout(char *cmd, char *buf, int len)
{
	FILE *fp;
	int size;
	fp = popen(cmd, "r"); 
	if(fp != NULL) {
		size = fread(buf, 1, len, fp);
		if(size <= 0)
			goto err;
		/* skip \n */
		buf[size - 1] = 0;
		pclose(fp);
		return;
	}
err:
	buf[0] = 0;
	sys_err("Exec %s failed: %s\n", cmd, strerror(errno));
	exit(0);
}

static int __uuid_equ(char *src, char *dest)
{
	return !strncmp(src, dest, UUID_LEN - 1);
}

static void __fill_msg_header(struct msg_head_t *msg, int msgtype)
{
	memcpy(&msg->acuuid[0], 
		&acuuid[0], UUID_LEN);
	memcpy(&msg->mac[0], 
		&argument.mac[0], ETH_ALEN);
	msg->msg_type = msgtype; 
}

static char *__buildcmd()
{
	char *cmd;
	cmd = malloc(100);
	strcpy(cmd, "cmdtest;");
	return cmd;
}

static void __ap_status(struct ap_t *ap, char *data)
{
	int ret;
	struct apstatus_t *status = (void *)(data + sizeof(struct msg_ap_status_t));
	printf("ssidnum:%d, ssid:%s \n", status->ssidnum, status->ssid0.ssid);

	char *cmd = __buildcmd();
	if(cmd == NULL) return;
	int cmdlen = strlen(cmd);
	int totallen = sizeof(struct msg_ac_cmd_t) + cmdlen;
	assert(totallen <= NET_PKT_DATALEN);

	struct msg_ac_cmd_t *msg = malloc(totallen);
	if(msg == NULL) {
		sys_warn("malloc msg for cmd failed: %s\n",
			strerror(errno));
		return;
	}
	__fill_msg_header(&msg->header, MSG_AC_CMD);
	strncpy((char *)msg + sizeof(struct msg_ac_cmd_t), cmd, cmdlen);

	struct nettcp_t tcp;
	tcp.sock = ap->sock;
	ret = tcp_sendpkt(&tcp, (char *)msg, totallen);
	if(ret <= 0)
		ap_lost(ap->sock);
	free(msg);
}

static void __ap_reg(struct ap_t *ap, struct msg_ap_reg_t *msg)
{
	pr_ap(&msg->header.mac[0], &msg->header.acuuid[0]);

	if(ap->isreg) {
		sys_warn("ap repeat register\n");
		return;
	}

	ap->isreg = 1;
	memcpy(&ap->mac[0], &msg->header.mac[0], ETH_ALEN);
	ap_reg_cnt++;
}

#define X86_UUID 	"cat /sys/class/dmi/id/product_uuid"
void msg_init()
{
	__get_cmd_stdout(X86_UUID, acuuid, UUID_LEN-1);
	acuuid[UUID_LEN - 1] = 0;
}

void ap_lost(int sock)
{
	delete_sockarr(sock);
}

void msg_proc(struct ap_hash_t *aphash, struct msg_head_t *msg)
{
	char *ap = msg->mac;

	if(!__uuid_equ(msg->acuuid, acuuid) 
		&& (msg->msg_type == MSG_AP_RESP)) {
			/* net other ap */
			pr_ap(ap, msg->acuuid);
			return;
	}

	switch(msg->msg_type) {
	case MSG_AP_REG:
		__ap_reg(&aphash->ap, (void *)msg);
		break;
	case MSG_AP_STATUS:
		__ap_status(&aphash->ap, (void *)msg);
		break;
	default:
		sys_err("Invaild msg type\n");
		break;
	}
}