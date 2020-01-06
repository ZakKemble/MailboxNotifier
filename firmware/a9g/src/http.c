/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

#define USERAGENT	"Mozilla/5.0 (compatible; Mail Notifier " FW_VERSION ")"

static void callback_dns(DNS_Status_t status, void* param)
{
	if(status == DNS_STATUS_OK)
		mail_sendEvent(MAILBOX_EVT_HTTP_BEGIN, 0, 0, NULL, NULL);
	else
		mail_sendEvent(MAILBOX_EVT_HTTP_DNSFAIL, 0, 0, NULL, NULL);
}

void http_begin()
{
	mail_sendEvent(MAILBOX_EVT_HTTP_BEGIN, 0, 0, NULL, NULL);
}

int http_host(char* server, uint32_t port)
{
	uint8_t ip[16];
	memset(ip, 0, sizeof(ip));

	DNS_Status_t status = DNS_GetHostByNameEX(server, ip, callback_dns, NULL); // TODO should probably pass the server name in param 4?
	if(status == DNS_STATUS_OK)
	{
		DBG_HTTP("Get IP success: %s -> %s", server, ip);
		DBG_HTTP("Connecting to %s %u...", ip, port);
		int fd = Socket_TcpipConnect(TCP, ip, port);
		if(fd < 0)
		{
			DBG_HTTP("socket fail %d", fd);
			return fd;
		}
		DBG_HTTP("Begin connect success...");
		return fd;
	}
	else if(status == DNS_STATUS_WAIT)
	{
		DBG_HTTP("Looking up... %s", server);
		return 0;
	}
	else
		DBG_HTTP("DNS Error %s", server);

	return -1;
}

int http_headerBegin(char* buff, char* reqType, char* host, char* uri)
{
	uint32_t idx = sprintf(buff, "%s %s HTTP/1.0\r\n", reqType, uri);
	idx += http_headerAdd(buff + idx, "Host", host);
	idx += http_headerAdd(buff + idx, "User-Agent", USERAGENT);
	return idx;
}

int http_headerAdd(char* buff, char* key, char* value)
{
	return sprintf(buff, "%s: %s\r\n", key, value);
}

int http_headerEnd(char* buff)
{
	return sprintf(buff, "\r\n");
}

int http_send(int fd, void* data, uint32_t len)
{
	// TODO loop Socket_TcpipWrite until all data has been written to socket, might block?
	return Socket_TcpipWrite(fd, data, len);
}

int http_read(int fd, void* data, uint32_t len)
{
	return Socket_TcpipRead(fd, data, len);
}

bool http_close(int fd)
{
	bool res = Socket_TcpipClose(fd);
	return res;
}
