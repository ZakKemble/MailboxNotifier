/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __HTTP_H_
#define __HTTP_H_

void http_begin(void);
int http_host(char* server, uint32_t port);
int http_headerBegin(char* buff, char* reqType, char* host, char* uri);
int http_headerAdd(char* buff, char* key, char* value);
int http_headerEnd(char* buff);
int http_send(int fd, void* data, uint32_t len);
int http_read(int fd, void* data, uint32_t len);
bool http_close(int fd);

#endif
