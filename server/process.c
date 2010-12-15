/**
 * @file process.c
 * process std-forwarding tunnel
 */
/*
 * This file is part of rdp2tcp
 *
 * Copyright (C) 2010-2011, Nicolas Collignon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "r2twin.h"
#include "print.h"
#include "rdp2tcp.h"

#include <stdio.h>

/** default stdin/stdout pipes name prefix */
#define PIPE_NAME "r2tcmd"

static void pipe_close(HANDLE *pfd)
{
	CloseHandle(pfd[0]);
	CloseHandle(pfd[1]);
}

static int pipe_create(HANDLE *pfd, int parent_fd)
{
	HANDLE fd;
	SECURITY_ATTRIBUTES sattr;
	char name[128];

	memset(&sattr, 0, sizeof(sattr));
	sattr.nLength = sizeof(sattr);
	sattr.bInheritHandle = TRUE;

	snprintf(name, sizeof(name)-1, "\\\\.\\pipe\\" PIPE_NAME "-%lu-%i",
			GetCurrentProcessId(), rand());

	fd = CreateNamedPipeA(name,
			PIPE_ACCESS_INBOUND|FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE|PIPE_WAIT, 2, NETBUF_MAX_SIZE/2, NETBUF_MAX_SIZE/2,
			5000 /*msec*/, &sattr);
	if (fd == INVALID_HANDLE_VALUE)
		return syserror("CreateNamedPipe");

	pfd[0] = fd;

	fd = CreateFileA(name, GENERIC_WRITE, 0, &sattr, 
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fd == INVALID_HANDLE_VALUE) {
		syserror("CreateFile");
		CloseHandle(pfd[0]);
		return -1;
	}

	pfd[1] = fd;

	if (!SetHandleInformation(pfd[parent_fd], HANDLE_FLAG_INHERIT, 0)) {
		syserror("SetHandleInformation");
		pipe_close(pfd);
		return -1;
	}

	return 0;
}

static int start_child(
					const char *cmd,
					HANDLE *out_std,
					PROCESS_INFORMATION *pi,
					unsigned char *err)
{
	BOOL res;
	HANDLE stderr_child, pstdin[2], pstdout[2];
	STARTUPINFOA si;

	trace_proc("%s", cmd);
	*err = R2TERR_GENERIC;

	// stdin pipe
	if (pipe_create(pstdin, 1))
		return -1;

	// stdout pipe
	if (pipe_create(pstdout, 0)) {
		pipe_close(pstdin);
		return -1;
	}

	// stderr pipe
	stderr_child = INVALID_HANDLE_VALUE;
	if (DuplicateHandle(GetCurrentProcess(), pstdout[1],
								GetCurrentProcess(), &stderr_child,
								0, TRUE, DUPLICATE_SAME_ACCESS)) {

		memset(pi, 0, sizeof(*pi));
		memset(&si, 0, sizeof(si));
		si.cb         = sizeof(si);
		si.dwFlags    = STARTF_USESTDHANDLES;
		si.hStdInput  = pstdin[0];
		si.hStdOutput = pstdout[1];
		si.hStdError  = stderr_child;

		res = CreateProcessA(NULL, (char*)cmd, NULL, NULL, TRUE, 0,
									NULL, NULL, &si, pi);
		if (res) {
			CloseHandle(stderr_child);
			CloseHandle(pi->hThread);
			CloseHandle(pstdin[0]);
			CloseHandle(pstdout[1]);
			out_std[0] = pstdout[0];
			out_std[1] = pstdin[1];
			return 0;
		}

		switch (GetLastError()) {
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				*err = R2TERR_NOTFOUND;
				break;

			default:
				syserror("CreateProcess");
		}

		CloseHandle(stderr_child);

	} else {
		syserror("DuplicateHandle");
	}

	pipe_close(pstdout);
	pipe_close(pstdin);

	return -1;
}

/**
 * spawn a child process and attach stdin/stdout/stderr to tunnel
 * @param[in] tun the new tunnel
 * @param[in] cmd command line to execute
 * @return 0 on success
 */
int process_start(tunnel_t *tun, const char *cmd)
{
	int ret;
	unsigned int ans_len;
	unsigned long pid;
	HANDLE pstd[2];
	PROCESS_INFORMATION pi;
	r2tmsg_connans_t ans;

	trace_proc("tid=0x%02x cmd=%s", tun->id, cmd);

	memset(&ans, 0, sizeof(ans));
	ans_len = 1;

	ret = start_child(cmd, pstd, &pi, &ans.err);
	if (!ret) {

		if (!aio_init_forward(&tun->rio, &tun->wio, "proc")) {

			if (!event_add_process(pi.hProcess, tun->rio.io.hEvent,
										tun->wio.io.hEvent, tun->id)) {

				tun->rfd  = pstd[0];
				tun->wfd  = pstd[1];
				tun->proc = pi.hProcess;

				info(0, "started process %s with pid %u for tunnel 0x%02x",
						cmd, (unsigned int) pi.dwProcessId,  tun->id);

				ans.err  = R2TERR_SUCCESS;
				ans.af   = TUNAF_ANY;
				pid = htonl(pi.dwProcessId);
				memcpy(&ans.addr[0], &pid, 4);
				ans_len = 8;

			} else {
				aio_kill_forward(&tun->rio, &tun->wio);
			}
		}
	}

	if ((channel_write(R2TCMD_CONN, tun->id, &ans.err, ans_len) >= 0)
			&& (ans.err == R2TERR_SUCCESS)) {
		tun->connected = 1;
		return 0;
	}

	if (!ret) {
		event_del_tunnel(tun->id);
		TerminateProcess(pi.hProcess, 0);
		CloseHandle(pi.hProcess);
		CloseHandle(pstd[0]);
		CloseHandle(pstd[1]);
	}

	return error("failed to start process %s for tunnel 0x%02x", cmd, tun->id);
}

/**
 * stop a tunnel associated with a process
 * @param[in] tun the process tunnel
 */
void process_stop(tunnel_t *tun)
{
	TerminateProcess(tun->proc, 0);
	CloseHandle(tun->proc);
	CloseHandle(tun->rfd);
	CloseHandle(tun->wfd);
	aio_kill_forward(&tun->rio, &tun->wio);
}

